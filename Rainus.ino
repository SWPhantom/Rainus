#include <SD.h>
#include <Wire.h>
#include "RTClib.h"
#include "Arduino.h"
#include "esp_adc_cal.h"

// DHT20 Temperature/Humidity sensor
#define TEMPSENSOR true
#if TEMPSENSOR == true
  #include "DHT20.h"
  DHT20 DHT;
#endif

//// SPI/SD Card section
#define FSPI_MISO   5
#define FSPI_SCLK   6
#define FSPI_MOSI   7
#define FSPI_SS     10

// Rainus log file (Don't forget the leading slash! eg "/rainLog.txt")
const char filename[] = "/rainLog.txt";
File txtFile;

//// External wakeup pin
const int buttonPin = 2;
int buttonState = 0; 
#define BUTTON_PIN_BITMASK 0x000000004 // 2^33 in hex. defines GPIO pin 2

//// Extra bits
uint32_t chipId = 0;

//// RTC/Time/etc
#define RTC_CHECK_COMPILE_TIME true
RTC_PCF8523 rtc; // Adafruit 3.3v RTC (https://learn.adafruit.com/adafruit-pcf8523-real-time-clock/rtc-with-arduino)
DateTime compileTime;
DateTime now;
RTC_DATA_ATTR int bootCount = 0;
int TIMEZONE_OFFSET = -7;

//// Debug/Helpers
#define DEBUG true
void pr(char * input) {
  #if DEBUG == true
    Serial.println(input);
  #endif
}

// Print quick function. If DEBUG is on, pr() will print to serial out.
void pr(StringSumHelper input) {
  #if DEBUG == true
    Serial.println(input);
  #endif
}

void writeLog() {
  vTaskDelay(10);
  now = rtc.now();

  vTaskDelay(10);
  txtFile = SD.open(filename, FILE_APPEND);
  
  // CSV format:
  // chipId, timestamp (UTC), unixtime, secondstime, temp (C), humidity (%)
  // Example: 7598744,2022-09-19T16:11:03,1663603863,716919063
  txtFile.print(chipId);
  txtFile.print(',');
  txtFile.print(now.timestamp());
  txtFile.print(',');
  txtFile.print(now.unixtime());
  txtFile.print(',');
  txtFile.print(now.secondstime());
  #if TEMPSENSOR == true

    vTaskDelay(10);
    // Set default values for the temp/humidity sensor
    float default_value = -666.0;
    float humidity = default_value;
    float temperature = default_value;
    int status = -1;

    // Async request data read, sleep for 80ms, read data, and convert
    status = DHT.requestData();
    delay(80);
    DHT.readData();
    status = DHT.convert();

    // If the DHT read was successful, set the found values. If not, keep the default values
    if (status == DHT20_OK) {
      humidity = DHT.getHumidity();
      temperature = DHT.getTemperature();
    }

    pr("Humidity:");
    pr(humidity);
    pr("Temperature:");
    pr(temperature);

    // Write to log file
    txtFile.print(',');
    txtFile.print(temperature); // Temperature (c)
    txtFile.print(',');
    txtFile.print(humidity); // Humidity
  #endif
  txtFile.println();
  txtFile.close();
}

esp_err_t err;

void setup() {
  setCpuFrequencyMhz(40);

  Serial.begin(9600);
  while (!Serial);
  pr("");
  pr("Start!");

  // Deep sleep wakeup source initialization.
  // This board allows the mask to define pins 0-5, but no others.
  // BUTTON_PIN_BITMASK 0x000000004 // defines GPIO pin 2
  err = esp_deep_sleep_enable_gpio_wakeup(BUTTON_PIN_BITMASK, ESP_GPIO_WAKEUP_GPIO_HIGH);
  if (err != ESP_OK) {
    pr("esp_deep_sleep_enable_gpio_wakeup resulted in error:" + (int)err);
  }

  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);
  buttonState = 0;

  // SPI setup for SD card
  SPIClass * fspi = new SPIClass(FSPI);
  fspi->begin(FSPI_SCLK, FSPI_MISO, FSPI_MOSI, FSPI_SS); //SCLK, MISO, MOSI, SS

  // Initialize the SD card
  if (!SD.begin(FSPI_SS, *fspi)) {
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

  //// RTC Setup part!
  // Check if RTC exists
  if (!rtc.begin()) {
    pr("Couldn't find RTC");
    // BROKEN. WE DONE
    esp_deep_sleep_start();
  }

  // Check the compile time of the sketch
  compileTime = DateTime(F(__DATE__), F(__TIME__));
  pr("Compile Date and time:");
  pr(__DATE__);
  pr(__TIME__);

  compileTime = DateTime(compileTime.unixtime() - (TIMEZONE_OFFSET * 60 * 60));
  // Check the time the RTC thinks it is
  pr("Checking PCF8523 RTC.\nCurrentTime:");
  now = rtc.now();
  pr(now.timestamp());

  // If the RTC is not initialized or if the compile time is later than the current chip time, adjust the RTC's clock
  if (!rtc.initialized() || compileTime > now) {
    pr("PCF8523 RTC is NOT initialized, let's set the time!");
    pr("Initializing the RTC...");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(compileTime);
    rtc.start();

    pr("...done!");
  }
    
  pr("RTC is running. Datetime:");
  pr(rtc.now().timestamp());

  // Begin the Temperature/Humidity sensor
  #if TEMPSENSOR == true
    DHT.begin();
  #endif
}

esp_sleep_wakeup_cause_t wakeup_reason;

void loop() {
  wakeup_reason = esp_sleep_get_wakeup_cause();

  // If the board woke up from deep sleep via rain gauge/button, we're golden! Time to log!
  if(wakeup_reason == 7) {
    pr("Button has been pressed. Time to LOG.");

    // Log to SD card!
    writeLog();

    // Section to wait until the button is unpressed, to not have bounced signal/logs that happen very quickly.
    do {
      buttonState = digitalRead(buttonPin);
    } while (buttonState == HIGH);
  }

  pr("The time has come for slumber...");
  esp_deep_sleep_start();
  // Program won't get here
}