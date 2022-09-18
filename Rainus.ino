#include <SD.h>
#include <Wire.h>
#include "RTClib.h"

//// RTC Section
RTC_DS1307 rtc;

//// SPI/SD Card section
#define FSPI_MISO   2
#define FSPI_MOSI   4
#define FSPI_SCLK   5
#define FSPI_SS     6

// Rainus log file (Don't forget the leading slash! eg "/rainLog.txt")
const char filename[] = "/rainLog.txt";
File txtFile;

//// Extra bits
uint32_t chipId = 0;

////
void setup() {
  Serial.begin(9600);
  while (!Serial);
  Serial.println();
  Serial.println("Start!");

  // SPI setup
  SPIClass * fspi = new SPIClass(FSPI);
  fspi->begin(FSPI_SCLK, FSPI_MISO, FSPI_MOSI, FSPI_SS); //SCLK, MISO, MOSI, SS
  pinMode(fspi->pinSS(), OUTPUT); //VSPI SS

  // Initialize the SD card
  if (!SD.begin(FSPI_SS,*fspi)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    while (1);
  } else {
    Serial.println("Card Detected!");
  }

  // Calculate chipId
  for(int i=0; i<17; i=i+8) {
	  chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
	}

  // try to open the file for writing
  txtFile = SD.open(filename, FILE_WRITE);
  Serial.print("FILEPATH::");
  Serial.println(txtFile.path());
  if (!txtFile) {
    Serial.print("error opening ");
    Serial.println(filename);
    while (1);
  }

  // add some new lines to start
  txtFile.println();
  txtFile.println("Hello World!");
  txtFile.close();
  Serial.println("Files are supposed to have leading slashes.");
}

int i = 1;
void loop() {  
  //sp_deep_sleep_start();
  //GPIO_NUM_X = rainGaugeGpio;
  //esp_sleep_enable_ext0_wakeup(GPIO_NUM_X, 1);

  DateTime now = rtc.now();

  if(i < 5) {
    txtFile = SD.open(filename, FILE_APPEND);
    Serial.println(i);
    txtFile.println(i++);
    txtFile.println(now.year() + '-' + now.month() + '-' + now.day() + "::" + now.hour() + ':' + now.minute() + ':' + now.second));
    txtFile.close();
    delay(1000);
  }
}

