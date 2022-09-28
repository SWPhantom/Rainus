#include <SD.h>
#include <Wire.h>
#include "RTClib.h"

//// SPI/SD Card section
#define FSPI_MISO   5
#define FSPI_MOSI   7
#define FSPI_SCLK   6
#define FSPI_SS     10

// Rainus log file (Don't forget the leading slash! eg "/rainLog.txt")
const char filename[] = "/rainLog.txt";
File txtFile;

//// Rain Gauge
const int buttonPin = 2;
int buttonState = 0; 
#define BUTTON_PIN_BITMASK 0x000000004 // 2^33 in hex. defines GPIO pin 2

//// Extra bits
uint32_t chipId = 0;

//// RTC/Time/etc
// NOTE: ONE XOR THE OTHER MUST BE true
#define PCF8523 true
#define DS1307 false

#if PCF8523 == true
  RTC_PCF8523 rtc; // Adafruit 3.3v RTC (https://learn.adafruit.com/adafruit-pcf8523-real-time-clock/rtc-with-arduino)
#endif
#if DS1307 == true
  RTC_DS1307 rtc; // RTC that uses 5v (but works with 3.3v...)
#endif
DateTime compileTime;
DateTime now;
RTC_DATA_ATTR int bootCount = 0;

//// Debug/Helpers
#define DEBUG true
void pr(char * input) {
  if(DEBUG){
    Serial.println(input);
  }
}

// To measure time!
#define TIMERS false
#if TIMERS == true && DEBUG == true
  long int t1;
  long int t2;
#endif

// Print quick function. If DEBUG is on, pr() will print to serial out.
void pr(StringSumHelper input) {
  #if DEBUG == true
    Serial.println(input);
  #endif
}

void setup() {
  #if TIMERS == true && DEBUG == true
    t1 = millis();
  #endif

  Serial.begin(9600);
  while (!Serial);
  pr("");
  pr("Start!");

  // Deep sleep wakeup source initialization.
  // This board allows the mask to define pins 0-5, but no others.
  // BUTTON_PIN_BITMASK 0x000000004 // defines GPIO pin 2
  esp_deep_sleep_enable_gpio_wakeup(BUTTON_PIN_BITMASK, ESP_GPIO_WAKEUP_GPIO_HIGH);

  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);
  buttonState = 0;

  //Increment boot number and print it every reboot
  ++bootCount;
  pr("Boot number: " + String(bootCount));

  // SPI setup
  SPIClass * fspi = new SPIClass(FSPI);
  fspi->begin(FSPI_SCLK, FSPI_MISO, FSPI_MOSI, FSPI_SS); //SCLK, MISO, MOSI, SS
  pinMode(fspi->pinSS(), OUTPUT); //VSPI SS

  // Initialize the SD card
  if (!SD.begin(FSPI_SS,*fspi)) {
    pr("Card failed, or not present");
    // If card isn't found, deep sleep.
    esp_deep_sleep_start();
  } else {
    pr("Card Detected!");
  }

  // Calculate chipId
  for(int i=0; i<17; i=i+8) {
	  chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
	}

  // Check if RTC exists
  // Universal across both RTCs
  if (!rtc.begin()) {
    pr("Couldn't find RTC");
    // BROKEN. WE DONE
    esp_deep_sleep_start();
  }

  // TODO: If RTC has fault, we may want to write to the log to tell researcher that
  // data may be faulty after a certain point (especially if we reflash, and have sane,
  // but invalid, time)

  // If the RTC is uninitialized, initialize it with the compilation time
  compileTime = DateTime(F(__DATE__), F(__TIME__));

  // Logic for 8523 RTC
  #if PCF8523 == true
    if (!rtc.initialized() || rtc.lostPower()) {
      pr("PCF8523 RTC is NOT initialized, let's set the time!");
      pr("Initializing the RTC...");
      // following line sets the RTC to the date & time this sketch was compiled
      rtc.adjust(compileTime);

      rtc.start();
    }
  #endif

  // Logic for 1307 RTC
  #if DS1307 == true
    if (!rtc.isrunning() ) {
      pr("DS1307 RTC is NOT initialized, let's set the time!");
      pr("Initializing the RTC...");

      // following line sets the RTC to the date & time this sketch was compiled
      rtc.adjust(compileTime);
    }
  #endif
    
  pr("RTC is running. Datetime:");
  pr(rtc.now().timestamp());
}

void loop() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  // If the board woke up from deep sleep via rain gauge/button, we're golden! Time to log!
  if(wakeup_reason == 7) {
    pr("Button has been pressed. Time to LOG.");

    now = rtc.now();
    txtFile = SD.open(filename, FILE_APPEND);
    
    // CSV format:
    // chipId, timestamp, unixtime, secondstime, (temp), (humidity), (batteryLevel)
    // Example: 7598744,2022-09-19T16:11:03,1663603863,716919063
    // TODO: Build string ahead of time. 
    txtFile.print(chipId);
    txtFile.print(',');
    txtFile.print(now.timestamp());
    txtFile.print(',');
    txtFile.print(now.unixtime());
    txtFile.print(',');
    txtFile.print(now.secondstime());    
    txtFile.println();
  
    txtFile.close();

    // Section to wait until the button is unpressed, to not have bounced signal/logs that happen very quickly.
    do {
      buttonState = digitalRead(buttonPin);
    } while (buttonState == HIGH);

  } else {
    pr("Here from reset/startup. Do nothing.");
  }
  
  #if TIMERS == true && DEBUG == true
    t2 = millis();
    pr("Time Elapsed for Operation(ms): ");
    pr(t2-t1);
  #endif

  esp_deep_sleep_start();
  pr("SENITEE CHEK SHOULD NOT PRINT.");
}