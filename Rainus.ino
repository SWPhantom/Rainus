#include <SD.h>
#include <Wire.h>
#include "RTClib.h"

//// RTC Section
RTC_DS1307 rtc;

//// SPI/SD Card section
#define FSPI_MISO   7
#define FSPI_MOSI   4
#define FSPI_SCLK   5
#define FSPI_SS     6

// Rainus log file (Don't forget the leading slash! eg "/rainLog.txt")
const char filename[] = "/rainLog.txt";
File txtFile;

//// Extra bits
uint32_t chipId = 0;
DateTime now;
int buttonState = 0; 
RTC_DATA_ATTR int bootCount = 0;
#define BUTTON_PIN_BITMASK 0x000000004 // 2^33 in hex

// debug 
long int t1;
long int t2;
////
void setup() {
  t1 = millis();
  // Initialize Serial (only for debug?)
  Serial.begin(9600);
  while (!Serial);
  Serial.println();
  Serial.println("Start!");

  // Deep sleep wakeup source initialization.
  // This board allows the mask to define pins 0-5, but no others.
  // BUTTON_PIN_BITMASK 0x000000004 // defines GPIO pin 2
  esp_deep_sleep_enable_gpio_wakeup(BUTTON_PIN_BITMASK, ESP_GPIO_WAKEUP_GPIO_HIGH);

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  // SPI setup
  SPIClass * fspi = new SPIClass(FSPI);
  fspi->begin(FSPI_SCLK, FSPI_MISO, FSPI_MOSI, FSPI_SS); //SCLK, MISO, MOSI, SS
  pinMode(fspi->pinSS(), OUTPUT); //VSPI SS

  // Initialize the SD card
  if (!SD.begin(FSPI_SS,*fspi)) {
    Serial.println("Card failed, or not present");
    // If card isn't found, deep sleep.
    esp_deep_sleep_start();
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
    esp_deep_sleep_start();

  }

  // TODO: If RTC has fault, we may want to write to the log to tell researcher that
  // data may be faulty after a certain point (especially if we reflash, and have sane,
  // but invalid, time)

  // If the RTC is uninitialized, initialize it with the compilation time
  if (!rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  // If the board woke up from deep sleep via rain gauge/button, we're golden! Time to log!
  if(wakeup_reason == 7) {
    Serial.println("Button has been pressed. Time to LOG.");

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
  } else {
    Serial.println("Here from reset/startup. Do nothing.");
  }
  t2 = millis();
  Serial.print("Time Elapsed for Operation(ms): ");
  Serial.println(t2-t1);
  Serial.println();
  esp_deep_sleep_start();
  Serial.println("SENITEE CHEK SHOULD NOT PRINT.");
}