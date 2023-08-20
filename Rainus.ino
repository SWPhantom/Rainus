#include <SD.h>
#include <Wire.h>
#include "RTClib.h"
#include "Arduino.h"
#include "esp_adc_cal.h"

#define TEMPSENSOR true
#if TEMPSENSOR == true
  #include "DHT20.h"
  DHT20 DHT;
#endif

#define WEB true
#if WEB == true
  #include <WiFi.h>
  #include <HTTPClient.h>
#endif

//// SPI/SD Card section
#define FSPI_MISO   5
#define FSPI_SCLK   6
#define FSPI_MOSI   7
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
#define RTC_CHECK_COMPILE_TIME true
RTC_PCF8523 rtc; // Adafruit 3.3v RTC (https://learn.adafruit.com/adafruit-pcf8523-real-time-clock/rtc-with-arduino)
DateTime compileTime;
DateTime now;
RTC_DATA_ATTR int bootCount = 0;

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

// To measure time!
#define TIMERS false
#if TIMERS == true && DEBUG == true
  long int t1;
  long int t2;
#endif

void writeLog() {
  now = rtc.now();

  txtFile = SD.open(filename, FILE_APPEND);
  
  // CSV format:
  // chipId, timestamp, unixtime, secondstime, (temp (C)), (humidity)
  // Example: 7598744,2022-09-19T16:11:03,1663603863,716919063
  // TODO: Build string ahead of time. 
  txtFile.print(chipId);
  txtFile.print(',');
  txtFile.print(now.timestamp());
  txtFile.print(',');
  txtFile.print(now.unixtime());
  txtFile.print(',');
  txtFile.print(now.secondstime());
  #if TEMPSENSOR == true
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

#if WEB == true
  String composeLog(uint32_t chipId, String logTimestamp, int bootCount, String compileTimestamp) {
    String pre1 = "{\"user\":\"";
    String pre2 = "\",\"collection\":\"";
    String pre3 = "\",\"datetime\":\"";
    String pre4 = "\",\"data\":\"BootNumber:";
    String pre5 = ", logTimestamp: ";
    String pre6 = "\"}";
    return pre1 + "rainus_profiler" + pre2 + String(chipId) + pre4 + String(bootCount) + pre5 + logTimestamp +pre6;
  }

  String ssid = "";
  String password = "";
  String serverPath = "";
  HTTPClient http;
  String payload;
  int httpResponseCode;

  void sendLog() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      retries++;
    }
    pr("Connected after this many 100ms retries:");
    pr(retries);
    pr(WiFi.localIP().toString());

    // Send log to server
    pr("About to send to");
    pr(serverPath);
    http.begin(serverPath.c_str());
    http.addHeader("Content-Type", "application/json");

    payload = composeLog(chipId, now.timestamp(), bootCount, compileTime.timestamp());
    
    pr("POSTing payload:");
    pr(payload);
    httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      pr("HTTP Response code: ");
      pr(httpResponseCode);
      String payload = http.getString();
      pr(payload);
    }
    else {
      pr("Error code: ");
      pr(httpResponseCode);
    }
    // Free resources
    http.end();
    WiFi.disconnect();
  }
#endif

esp_err_t err;

void setup() {
  setCpuFrequencyMhz(40);
  
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
  err = esp_deep_sleep_enable_gpio_wakeup(BUTTON_PIN_BITMASK, ESP_GPIO_WAKEUP_GPIO_HIGH);
  if (err != ESP_OK) {
    pr("esp_deep_sleep_enable_gpio_wakeup resulted in error:" + (int)err);
  }

  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);
  buttonState = 0;

  //Increment boot number and print it every reboot
  #if DEBUG == true
    ++bootCount;
    pr("Boot number: " + String(bootCount));
  #endif

  // SPI setup
  SPIClass * fspi = new SPIClass(FSPI);
  fspi->begin(FSPI_SCLK, FSPI_MISO, FSPI_MOSI, FSPI_SS); //SCLK, MISO, MOSI, SS
  // pinMode(fspi->pinSS(), OUTPUT); //VSPI SS

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
  pr("Checking PCF8523.\nCurrentTime:");
  pr(rtc.now().timestamp());

  if (!rtc.initialized()) {
    pr("PCF8523 RTC is NOT initialized, let's set the time!");
    pr("Initializing the RTC...");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(compileTime);
    rtc.start();

    pr("...done!");
  }

  #if RTC_CHECK_COMPILE_TIME == true
    now = rtc.now();
    if (compileTime > now) {
      pr("RTC time behind CompileTime. Resetting");
      rtc.adjust(compileTime);
      rtc.start();
  }
  #endif
    
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

    // Connect to wifi and send log to loggo
    #if WEB == true
      sendLog();
    #endif

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

  pr("sleep time!");
  esp_deep_sleep_start();
  pr("SENITEE CHEK SHOULD NOT PRINT.");
}