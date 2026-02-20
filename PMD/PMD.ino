//Random bools
bool DevDebug = 1; //To send more things over serial and on screen more stuff
bool NoScreen = 0; //Just for debugging when screen is not connected
bool BlockWriting = 0; //Block writing to SD when its full
bool SDSuccess = 0; //Indicator that SD is connected and mounted so stuff can be done with it
//SD Card
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#define REASSIGN_SD_PINS
int sck = 12;
int miso = 13;
int mosi = 11;
int cs = 10;
//Bluetooth
#include <NimBLEDevice.h>

NimBLECharacteristic *g_passwordCharacteristic = nullptr;
uint32_t new_pin = 0;
// --- Helper Functions ---

// --- Callback Classes ---

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override {
    new_pin = esp_random() % 1000000;
    NimBLEDevice::setSecurityPasskey(new_pin);
    Serial.println(new_pin);
    Serial.print("Client connected: ");
    Serial.println(connInfo.getAddress().toString().c_str());
  }

  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override {
    Serial.println("Client disconnected. Restarting advertising...");
    NimBLEDevice::startAdvertising();
  }

  uint32_t onPassKeyDisplay() override {
    Serial.print("Passkey (enter on phone): ");
    Serial.println(new_pin);
    return new_pin;
  }

  void onConfirmPassKey(NimBLEConnInfo &connInfo, uint32_t pass_key) override {
    Serial.print("Confirming passkey: ");
    Serial.println(pass_key);
    // Automatically inject confirmation for numeric comparison
    NimBLEDevice::injectConfirmPasskey(connInfo, true);
  }

  void onAuthenticationComplete(NimBLEConnInfo &connInfo) override {
    if (connInfo.isEncrypted() && connInfo.isAuthenticated()) {
      Serial.println("Secure connection established (Encrypted + MITM Protection)");
    } else {
      Serial.println("Security failed; forcing disconnect.");
      NimBLEDevice::getServer()->disconnect(connInfo.getConnHandle());
    }
  }
};

class PasswordCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {
    std::string value = pCharacteristic->getValue();
    if (!value.empty()) {
      Serial.print("Received password: ");
      Serial.println(value.c_str());
    } else {
      Serial.println("Password write was empty");
    }
  }
};
//I2C display
#include <Wire.h>
#define OLED_SDA 4
#define OLED_SCL 5
#include <U8g2lib.h>
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
//AES Encyption
#include <string.h>
#include <aes/esp_aes.h>
// AES-256 Key (replace)
#define AES_KEY     0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, \
                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, \
                    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, \
                    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
// IV => should be a real random value
#define AES_IV      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, \
                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
char PlainTextBuffer[1024] = {'\0'};
char encrypted[1024] = {'\0'};


void encodetest(){
	uint8_t key[32] = {AES_KEY};
	uint8_t encryptionIV[16] = {AES_IV};
	uint8_t decryptionIV[16] = {AES_IV};

	memset( PlainTextBuffer, 0, sizeof( PlainTextBuffer ) );
	strcpy( PlainTextBuffer, "Hello, ESP32!" );

	esp_aes_context ctx;
	esp_aes_init( &ctx );
	esp_aes_setkey( &ctx, key, 256 );
 	esp_aes_crypt_cbc( &ctx, ESP_AES_ENCRYPT, sizeof(PlainTextBuffer), encryptionIV, (uint8_t*)PlainTextBuffer, (uint8_t*)encrypted );

	//See encrypted payload, and wipe out PlainTextBuffer.
	memset( PlainTextBuffer, 0, sizeof( PlainTextBuffer ) );
	int i;
	for( i = 0; i < 16; i++ )
	{
		Serial.println(encrypted[i]);
	}

	//Use the ESP32 to decrypt the CBC block.
	esp_aes_crypt_cbc( &ctx, ESP_AES_DECRYPT, sizeof(encrypted), decryptionIV, (uint8_t*)encrypted, (uint8_t*)PlainTextBuffer );

	//Verify output
	for( i = 0; i < 16; i++ )
	{
		Serial.println(PlainTextBuffer[i]);
	}
}


void StartSD(){
  SPI.begin(sck, miso, mosi, cs);
  short int DotCounter = 0;
  if(!SD.begin(cs)){
    u8g2.clearBuffer();
    u8g2.drawStr(0,24,"Card Mount Failed");
    u8g2.sendBuffer();
    while(!SD.begin(cs)){
      u8g2.setCursor(104,24);
      Serial.println("Card Mount Failed");
      for(int i =0; i<DotCounter; i++){
        u8g2.print(".");
      }
      u8g2.sendBuffer();
      DotCounter++;
      if(DotCounter==4){
        DotCounter = 0;
        delay(500);
        u8g2.setDrawColor(0);
        u8g2.drawBox(104,24-8,25,8);
        u8g2.setCursor(104,24);
        u8g2.setDrawColor(1);
        u8g2.sendBuffer();
      }
      delay(200);
    }
    u8g2.clearBuffer();
    u8g2.clearDisplay();
  }
  if(DevDebug){
    u8g2.clearBuffer();
    u8g2.drawStr(0,24,"Screen Init");
    u8g2.sendBuffer();
  }
  uint8_t cardType = SD.cardType();
  u8g2.setCursor(0,8);
  if(cardType == CARD_MMC){
    u8g2.print("MMC");
    if(DevDebug) Serial.println("Card Type is MMC");
  } else if(cardType == CARD_SD){
    u8g2.print("SDSC");
    if(DevDebug) Serial.println("Card Type is SDSC");
  } else if(cardType == CARD_SDHC){
    u8g2.print("SDHC");
    if(DevDebug) Serial.println("Card Type is SDHC");
  } else {
    u8g2.print("UNKNOWN");
    if(DevDebug) Serial.println("Card Type is UNKNOWN");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  u8g2.printf("     %lluMB\n", cardSize);
  u8g2.sendBuffer();
  if(SD.totalBytes() < SD.usedBytes()+32768){
    BlockWriting = 1;
    if(DevDebug){
      Serial.println("Blocked writing due to low storage");
      Serial.print("Used Bytes: ");
      Serial.println(SD.usedBytes());
      Serial.print("Total Bytes: ");
      Serial.println(SD.totalBytes());
    }
  }
  Serial.println("Blocked writing due to low storage");
  Serial.print("Used Bytes: ");
  Serial.println(SD.usedBytes());
  Serial.print("Total Bytes: ");
  Serial.println(SD.totalBytes());
  SDSuccess = 1;
}

TaskHandle_t CoreFunc; //Task Handle for multithreading

void setup() {  
  Serial.begin(115200);
  while(DevDebug && !Serial){
		delay(10);
  }
  Wire.begin(OLED_SDA, OLED_SCL);
  delay(100);
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  if(1){ //Just retract bluetooth shit

  NimBLEDevice::init("ESP32-S3-Secret");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  /** * Security Setup:
   * bonding = true, mitm = true, secure connections = true 
   */
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  NimBLEDevice::setSecurityPasskey(new_pin);

  NimBLEServer *server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  // Service UUID
  NimBLEService *service = server->createService("f0e1d2c3-b4a5-4678-9abc-def012345678");

  // Characteristic UUID - Requires encryption and authentication (MITM) to write
  g_passwordCharacteristic = service->createCharacteristic(
      "f0e1d2c3-b4a5-4678-9abc-def012345679",
      NIMBLE_PROPERTY::WRITE | 
      NIMBLE_PROPERTY::WRITE_ENC | 
      NIMBLE_PROPERTY::WRITE_AUTHEN
  );
  g_passwordCharacteristic->setCallbacks(new PasswordCallbacks());

  service->start();

  // Advertising Setup
  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->setName("ESP32-S3");
  advertising->addServiceUUID(service->getUUID());
  advertising->enableScanResponse(true);
  advertising->start();

  Serial.println("Advertising started.");
  }
  xTaskCreatePinnedToCore (
    CoreFunccode,     // Function to implement the task
    "CoreFunc",   // Name of the task
    4096,      // Stack size in bytes
    NULL,      // Task input parameter
    1,         // Priority of the task
    &CoreFunc,      // Task handle.
    0          // Core where the task should run
  );
  StartSD();


}

void loop() {
  if (BlockWriting == 1) {
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.setDrawColor(1); // Draw new text
    u8g2.setCursor(0, 63);
    u8g2.print("W:low free space");
    u8g2.print((SD.totalBytes()-SD.usedBytes())/1024/1024);
    u8g2.print("MB");
    u8g2.sendBuffer();
  }
  //Add check if sd card is ejected
}
void CoreFunccode(void * pvParameters ) {
  
  encodetest();
  while(1){
    if(SDSuccess){ //When SD Card is mounted
      if (DevDebug) Serial.println("SD Card is mounted successfully, starting read and decrypt function");

    }
    delay(10000);
  }
}



//Fuctions for intercating with SD card
void renameFile(fs::FS &fs, const char *path1, const char *path2, bool IsCriticalRename) {
  if(DevDebug){
    Serial.printf("Renaming file %s to %s\n", path1, path2);
  }
  if (fs.rename(path1, path2)) {
    
    if(DevDebug){
      Serial.println("File renamed");
    }
  } 
  else {
    if(IsCriticalRename){
      if(DevDebug){
        Serial.println("CRITICAL RENAME FAILED");
      }
    }
    if(DevDebug){
      Serial.println("Rename failed");
    }
  }
}

void deleteFile(fs::FS &fs, const char *path) {
  if(DevDebug){
    Serial.printf("Deleting file: %s\n", path);
  }
  if (fs.remove(path)) {
    if(DevDebug){
      Serial.println("File deleted");
    }
  } 
  else {
    if(DevDebug){
      Serial.println("Delete failed");
    }
  }
}

void appendFile(fs::FS &fs, const char *path, const char *message) {
  if(DevDebug){
    Serial.printf("Appending to file: %s\n", path);
  }
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    if(DevDebug){
      Serial.println("Failed to open file for appending");
    }
    return;
  }
  if (file.print(message)) {
    if(DevDebug){
      Serial.println("Message appended");
    }
  } else {
    if(DevDebug){
      Serial.println("Append failed");
    }
  }
  file.close();
}