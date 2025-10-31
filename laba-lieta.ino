const char* ntpServer = "ntp.cert.lv"; //ntp server for tyme sync done every hour
const long  gmtOffset_sec = 7200; //Time offset in seconds, for Riga +2*3600
const int   daylightOffset_sec = 3600; //Offest for Winter/Summer time to work
unsigned short int ServerStatCount = 50; //How many PC's will send their stats to esp32
/*
CODE
*/
#include "secrets.h"
#include <WiFi.h>
#include <Wire.h>
#include "time.h"
#include <U8g2lib.h>
#include <ArduinoJson.h>

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

/* ---------------- SETUP ---------------- */
void setup() {
  u8g2.begin();  // Initialize display
  Serial.begin(115200);
  xTaskCreatePinnedToCore(UpdateScreenCode, "UpdateScreen", 2048, NULL, 9, &UpdateScreen, 0); //Loop for updating screen info. Shall run often so user doesnt constantly get lagged reaction to button presses.
  DataBuffer = new char*[ServerStatCount];
  for (int i = 0; i < ServerStatCount; i++) {
    DataBuffer[i] = new char[2048];
    memset(DataBuffer[i], 0, 2048);
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
  strncpy(DataBuffer[serverIndex - 1], jsonData.c_str(), 2047);
  DataBuffer[serverIndex - 1][2047] = '\0';

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
  double mem_used = doc["mem_used"] | 0.0;
  double mem_free = doc["mem_free"] | 0.0;
  Serial.printf("Memory used: %.0f bytes\n", mem_used);
  Serial.printf("Memory free: %.0f bytes\n", mem_free);

  int disk_used = doc["disk_home_used_percent"] | 0;
  const char* disk_free = doc["disk_home_free"] | "?";
  Serial.printf("Disk /home used: %d%% free: %s\n", disk_used, disk_free);

  const char* disk_dev = doc["disk_device"] | "?";
  float rKBs = doc["rKBs"] | 0.0;
  float wkBs = doc["wkBs"] | 0.0;
  Serial.printf("Disk I/O (%s): rKBs=%.2f wkBs=%.2f\n", disk_dev, rKBs, wkBs);

  if (doc.containsKey("net")) {
    JsonArray netArr = doc["net"].as<JsonArray>();
    for (JsonObject net : netArr) {
      const char* iface = net["iface"];
      float rx = net["rx_kBps"];
      float tx = net["tx_kBps"];
      Serial.printf("Net %s: RX=%.2f kB/s, TX=%.2f kB/s\n", iface, rx, tx);
    }
  }

  const char* timestamp = doc["timestamp"] | "?";
  Serial.printf("Timestamp: %s\n", timestamp);
  Serial.println("--------------------");

  client.stop();
  Serial.println("Client disconnected");
}
}

/* ---------------- SCREEN CODE ---------------- */

void DisplayStatsOnScreen(int DisplayingServerIndex){
  Serial.println(DisplayingServerIndex);
  for(int i=0; i<1024; i++){
    //Serial.print(DataBuffer[ServerStatIndex-1][i]);
    //Serial.print(DataBuffer[ServerStatIndex-2][i]);
    delay(1);
  }
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
    if(ScreenMode > 2){
      if(ScreenMode == 2){
        DisplayStatsOnScreen(2);
      }
    }
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
