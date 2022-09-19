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
const int buttonPin = 2; 
int buttonState = 0; 
RTC_DATA_ATTR int bootCount = 0;
#define BUTTON_PIN_BITMASK 0x000000004 // 2^33 in hex

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

////
void setup() {
  Serial.begin(9600);
  while (!Serial);
  Serial.println();
  Serial.println("Start!");

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  //Print the wakeup reason for ESP32
  print_wakeup_reason();

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

  Serial.println("In Sertup. Before checking buttonPin");

  // iIitialize the pushbutton pin as an input:
  /*
  First we configure the wake up source
  We set our ESP32 to wake up for an external trigger.
  There are two types for ESP32, ext0 and ext1 .
  ext0 uses RTC_IO to wakeup thus requires RTC peripherals
  to be on while ext1 uses RTC Controller so doesnt need
  peripherals to be powered on.
  Note that using internal pullups/pulldowns also requires
  RTC peripherals to be turned on.
  */
  esp_deep_sleep_enable_gpio_wakeup(BUTTON_PIN_BITMASK, ESP_GPIO_WAKEUP_GPIO_HIGH);
}

void loop() {
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
  
  esp_deep_sleep_start();
  Serial.println("SENITEE CHEK SHOULD NOT PRINT.");
}