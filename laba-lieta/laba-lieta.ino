const char* ntpServer = "ntp.cert.lv"; //ntp server for tyme sync done every hour
const long  gmtOffset_sec = 7200; //Time offset in seconds, for Riga +2*3600
const int   daylightOffset_sec = 0; //Offest for Winter/Summer time to work
int ServerStatCount = 130; //How many PC's will send their stats to esp32
/*
CODE
*/
#include "secrets.h"
#include <WiFi.h>
#include <Wire.h>
#include "time.h"
#include <U8g2lib.h>
#include <ArduinoJson.h>
#include <EncButton.h>

// Define screen 
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
/* --- TCP --- */
WiFiServer server(1234);  // TCP port
unsigned short int DataWriteIndex = 0; //Buffer write location for receiving tcp data 
//int FailedReceives = 0; // Int for counting Failed receives because of bufferoverflow
struct tm timeinfo; //For storing current time and date
short int ScreenMode = -1; // Different screen modes
/*
-1 = connecting to wifi
0 = Time
1 = Detailed Time
2 = Server1 Stats
2 = Detailed Server1 stats 1
3 = Detailed Server1 stats 2
4 = Server2 Stats
5 = Detailed Server2 stats 1
6 = Detailed Server2 stats 2
etc
*/
unsigned short int MaxForScreenMode = 0; // Different screen modes
bool IsConnectingToWifi = 0;
int ServerStatIndex = 0;
char** DataBuffer;  // pointer to array of char pointers
TaskHandle_t UpdateScreen; //Some shit for multithreading
TaskHandle_t ButtonDetector; //Some shit for multithreading


Button ButtonForward(4);
Button ButtonBack(5);




double* memory_used = new double[ServerStatCount];
double* memory_free = new double[ServerStatCount];
int* disk_used = new int[ServerStatCount];
const char** disk_free = new const char*[ServerStatCount];
const char** disk_dev = new const char*[ServerStatCount];
float* rKBs = new float[ServerStatCount];
float* wkBs = new float[ServerStatCount];
const char** iface = new const char*[ServerStatCount];
float* rx = new float[ServerStatCount];
float* tx = new float[ServerStatCount];
const char** timestamp = new const char*[ServerStatCount];
bool* IsDataReady = new bool[ServerStatCount];
bool* LockedByReading = new bool[ServerStatCount];
bool* HasBeenDisaplyedOnScreen = new bool[ServerStatCount];
float TempRx = 0.0;
float TempTx = 0.0;

/* ---------------- SETUP ---------------- */
void setup() {
  u8g2.begin();  // Initialize display
  Serial.begin(115200);
  xTaskCreatePinnedToCore(UpdateScreenCode, "UpdateScreen", 2048, NULL, 9, &UpdateScreen, 0); //Loop for updating screen info. Shall run often so user doesnt constantly get lagged reaction to button presses.
  xTaskCreatePinnedToCore(ButtonDetectorCode, "ButtonDetector", 2048, NULL, 1, &ButtonDetector, 0); //Loop for updating screen info. Shall run often so user doesnt constantly get lagged reaction to button presses.
  DataBuffer = new char*[ServerStatCount];
  for (int i = 0; i < ServerStatCount; i++) {
    DataBuffer[i] = new char[15];
    memset(DataBuffer[i], 0, 15);
  }
  Serial.println("Connecting to: ");
  Serial.print(ssid);
  WiFi.begin(ssid, password); //Connect to wifi

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(200 / portTICK_PERIOD_MS);  // Non-blocking delay of 1 second
    Serial.print(".");
    IsConnectingToWifi = 1;
  }
  ScreenMode = 0;
  IsConnectingToWifi = 0;
  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());

  server.begin();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void SetMaxForScreenMode() {
  MaxForScreenMode = (ServerStatCount * 3) + 1;
}

/* ---------------- LOOP ---------------- */
void loop() {
  WiFiClient client = server.available();
  if (client){

  Serial.println("New client connected!");

  // --- Step 1: Read index prefix ---
  String indexBuffer = "";
  while (client.connected() || client.available()) {
    if (client.available()) {
    char c = client.read();
    if (c == '|'){
      break;
    }
    indexBuffer += c;
    }
  }

  int serverIndex = indexBuffer.toInt();
  if (serverIndex <= 0 || serverIndex > ServerStatCount) {
    Serial.println("Invalid index from client.");
    client.stop();
    return;
  }
  Serial.printf("Receiving from client index: %d\n", serverIndex);

  // --- Step 2: Read full JSON payload ---
  String jsonData = "";
  while (client.connected() || client.available()) {
    while (client.available()) {
      char c = client.read();
      if (c != '\n' && c != '\r'){
        jsonData += c;
      }
    }
  }
  jsonData.trim();

  if (jsonData.length() == 0) {
    Serial.println("JSON parse error: EmptyInput");
    client.stop();
    return;
  }

  // --- Step 3: Store raw JSON in buffer ---

  // --- Step 4: Parse JSON ---
  StaticJsonDocument<3072> doc;
  DeserializationError error = deserializeJson(doc, jsonData);
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    client.stop();
    return;
  }


  // --- Step 5: Print parsed stats ---
  Serial.printf("--- Parsed stats from server %d ---\n", serverIndex);
  for (int i = 0; i < 32; i++) {
    String key = "cpu" + String(i);
    if (doc.containsKey(key)) {
      Serial.printf("CPU%-2d: %.2f%%\n", i, (float)doc[key]);
    }
  }
  //serverIndex -1
  memory_used[serverIndex-1] = doc["mem_used"] | 0.0;
  memory_free[serverIndex-1] = doc["mem_free"] | 0.0;

  disk_used[serverIndex-1] = doc["disk_home_used_percent"] | 0;
  disk_free[serverIndex-1] = doc["disk_home_free"] | "?";
  Serial.print(serverIndex-1);
  disk_dev[serverIndex-1] = doc["disk_device"] | "?";
  rKBs[serverIndex-1] = doc["rKBs"] | 0.0;
  wkBs[serverIndex-1] = doc["wkBs"] | 0.0;
  IsDataReady[serverIndex-1] = 1;
  if (doc.containsKey("net")) {
    JsonArray netArr = doc["net"].as<JsonArray>();
    for (JsonObject net : netArr) {
      iface[serverIndex-1] = net["iface"];
      rx[serverIndex-1] = net["rx_kBps"];
      tx[serverIndex-1] = net["tx_kBps"];
      Serial.printf("Net %s: RX=%.2f kB/s, TX=%.2f kB/s\n", iface[serverIndex-1], rx[serverIndex-1], tx[serverIndex-1]);
    }
  }

  timestamp[serverIndex-1] = doc["timestamp"] | "?";
  Serial.printf("Timestamp: %s\n", timestamp[serverIndex-1]);
  Serial.println("--------------------");

  client.stop();
  Serial.println("Client disconnected");
}
}

/* ---------------- SCREEN CODE ---------------- */

void DisplayStatsOnScreen(int DisplayingServerIndex){
  Serial.println(DisplayingServerIndex);
  //Serial.printf("Memory used: %.0f bytes\n", memory_used[3-1]);
  //Serial.printf("Memory free: %.0f bytes\n", memory_free[3-1]);
  //Serial.printf("Disk /home used: %d%% free: %s\n", disk_used[3-1], disk_free[3-1]);
  //Serial.printf("Disk I/O (%s): rKBs=%.2f wkBs=%.2f\n", disk_dev[3-1], rKBs[3-1], wkBs[3-1]);
  Serial.printf("Net %s: RX=%.2f kB/s, TX=%.2f kB/s\n", iface[3-1], rx[3-1], tx[3-1]);
    //Serial.print(DataBuffer[ServerStatIndex-1][i]);
    //Serial.print(DataBuffer[ServerStatIndex-2][i]);
    vTaskDelay(100);
}
void UpdateScreenCode( void *pvParameters){
  u8g2.clearBuffer();// Clear display buffer
  char ClockBuffer[8];
  char DateBuffer[20];
  char DayBuffer[10];
  int n =0;
  while(1){
    if(ScreenMode == -1){
      u8g2.setFont(u8g2_font_ncenB08_tr);  // Classic serif font
      u8g2.setCursor(0,10);
      u8g2.print("Connecting to wifi");
      u8g2.setCursor(0,20);
      u8g2.print(ssid);
      if(IsConnectingToWifi){
        n++;
        for(int i=0; i<n; i++){
          u8g2.print('.');
        }
        if(n > 3){
          n=-1;
        }
      }
      u8g2.sendBuffer();// Transfer buffer to display 
      u8g2.clearBuffer();// Clear display buffer
      vTaskDelay(75);// I love this animation speed
    }
    if(ScreenMode == 0){
      if(!getLocalTime(&timeinfo)){ // this check takes 5 seconds
        Serial.println("Failed to obtain time");
      }
    }
    if(ScreenMode == 0){ //Show just clock
    //u8g2_font_logisoso42_tn fits hh;mm
      u8g2.setFont(u8g2_font_logisoso28_tn);
      strftime(ClockBuffer, 9, "%H:%M:%S", &timeinfo);
      u8g2.drawStr(0, 28, ClockBuffer);
      u8g2.setFont(u8g2_font_logisoso16_tn);
      strftime(DateBuffer, 20, "%d.%m.%Y", &timeinfo);
      u8g2.drawStr(0, 48, DateBuffer);
      strftime(DayBuffer, 10, "%A", &timeinfo);
      u8g2.setFont(u8g2_font_ncenB08_tr);  // Classic serif font
      u8g2.drawStr(0, 60, DayBuffer);
      u8g2.sendBuffer();// Transfer buffer to display 
      u8g2.clearBuffer();// Clear display buffer
      vTaskDelay(100); 
    }
    if(ScreenMode == 1){ // Show detailed clock

    }
    if(ScreenMode > 1){
        DisplayStatsOnScreen(ScreenMode);
    }
    vTaskDelay(100);
  }
}

void ButtonDetectorCode( void *pvParameters){
  for(;;){
    ButtonForward.tick();
    ButtonBack.tick();
    if(ScreenMode == -1){
      vTaskDelay(100); //Disable buttons while connecting to wifi
    }
    else if(ButtonForward.press() && ScreenMode != MaxForScreenMode - 1){
      ScreenMode++;
      Serial.print(ScreenMode);
    }
    else if(ButtonBack.press() && ScreenMode != 0){
      Serial.print(ScreenMode);
      ScreenMode--;
    }
    vTaskDelay(100);
  }
}


void printLocalTime(){ // Useless just as an example
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.print("Day of week: ");
  Serial.println(&timeinfo, "%A");
  Serial.print("Month: ");
  Serial.println(&timeinfo, "%B");
  Serial.print("Day of Month: ");
  Serial.println(&timeinfo, "%d");
  Serial.print("Year: ");
  Serial.println(&timeinfo, "%Y");
  Serial.print("Hour: ");
  Serial.println(&timeinfo, "%H");
  Serial.print("Minute: ");
  Serial.println(&timeinfo, "%M");
  Serial.print("Second: ");
  Serial.println(&timeinfo, "%S");
}
