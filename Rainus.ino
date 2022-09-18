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
DateTime now;

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
    // BROKEN. WE DONE
    while (1);
  } else {
    Serial.println("Card Detected!");
  }

  // Calculate chipId
  for(int i=0; i<17; i=i+8) {
	  chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
	}

  // Check if RTC exists
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    // BROKEN. WE DONE
    while (1);
  }

  // If the RTC is uninitialized, initialize it with the compilation time
  if (!rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

int i = 1;
void loop() {  
  //sp_deep_sleep_start();
  //GPIO_NUM_X = rainGaugeGpio;
  //esp_sleep_enable_ext0_wakeup(GPIO_NUM_X, 1);

  
  if(i++ < 5){
    now = rtc.now();
    txtFile = SD.open(filename, FILE_APPEND);
    
    // CSV format:
    // chipId, timestamp, unixtime, secondstime, (temp), (humidity), (batteryLevel)
    // Example: ...
    txtFile.print(chipId);
    txtFile.print(',');
    txtFile.print(now.timestamp());
    txtFile.print(',');
    txtFile.print(now.unixtime());
    txtFile.print(',');
    txtFile.print(now.secondstime());    
    txtFile.println();
    
    txtFile.close();
    delay(1000);
  }
}