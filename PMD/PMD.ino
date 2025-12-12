#include <string.h>
#include <aes/esp_aes.h>

#include <Wire.h>

#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define OLED_SDA 4
#define OLED_SCL 5
#include <U8g2lib.h>
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
// AES-256 Key => replace with your own
#define AES_KEY     0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, \
                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, \
                    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, \
                    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F

// IV => should be a real random value
#define AES_IV      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, \
                    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F

bool DevDebug = 1;
char plaintext[64];
char encrypted[64];

bool BlockWriting = 0; //Block writing to SD when its full
bool SDSuccess = 0; //Indicator that SD is connected and mounted so stuff can be done with it




void encodetest(){
	uint8_t key[32] = {AES_KEY};
	uint8_t encryptionIV[16] = {AES_IV};
	uint8_t decryptionIV[16] = {AES_IV};

	memset( plaintext, 0, sizeof( plaintext ) );
	strcpy( plaintext, "Hello, ESP32!" );

	esp_aes_context ctx;
	esp_aes_init( &ctx );
	esp_aes_setkey( &ctx, key, 256 );
 	esp_aes_crypt_cbc( &ctx, ESP_AES_ENCRYPT, sizeof(plaintext), encryptionIV, (uint8_t*)plaintext, (uint8_t*)encrypted );

	//See encrypted payload, and wipe out plaintext.
	memset( plaintext, 0, sizeof( plaintext ) );
	int i;
	for( i = 0; i < 16; i++ )
	{
		Serial.println(encrypted[i]);
	}

	//Use the ESP32 to decrypt the CBC block.
	esp_aes_crypt_cbc( &ctx, ESP_AES_DECRYPT, sizeof(encrypted), decryptionIV, (uint8_t*)encrypted, (uint8_t*)plaintext );

	//Verify output
	for( i = 0; i < 16; i++ )
	{
		Serial.println(plaintext[i]);
	}
}


void StartSD(){
  if(!DevDebug){
  short int DotCounter = 0;
  if(!SD.begin()){
    u8g2.clearBuffer();
    u8g2.drawStr(0,24,"Card Mount Failed");
    u8g2.sendBuffer();
    while(!SD.begin()){
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
  }
  uint8_t cardType = SD.cardType();
  u8g2.setCursor(0,8);
  if(cardType == CARD_MMC){
    u8g2.print("MMC");
  } else if(cardType == CARD_SD){
    u8g2.print("SDSC");
  } else if(cardType == CARD_SDHC){
    u8g2.print("SDHC");
  } else {
    u8g2.print("UNKNOWN");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  u8g2.printf("     %lluMB\n", cardSize);
  u8g2.sendBuffer();
  if(!(SD.totalBytes() < SD.usedBytes()+32768)){
    BlockWriting = 1;
    Serial.println(SD.usedBytes());
    Serial.println(SD.totalBytes());
    Serial.println(SD.usedBytes());
  }
  SDSuccess = 1;
}
TaskHandle_t CoreFunc;
void setup() {  
  Wire.begin(OLED_SDA, OLED_SCL);
  Serial.begin(115200);
  delay(200);
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);
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
  encodetest();


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
}
void CoreFunccode(void * pvParameters ) {
  
  encodetest();
  while(1){
    delay(10000);
  }
}