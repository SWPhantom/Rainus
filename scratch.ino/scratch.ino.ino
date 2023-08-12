#include <SD.h>
#include <Wire.h>
#include "RTClib.h"
#include "Arduino.h"
#include "esp_adc_cal.h"

#define TEMPSENSOR true
#if TEMPSENSOR == true
  #include "DHT.h"
  #define DHTTYPE DHT20
#endif

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

esp_err_t err;

void setup() {
  setCpuFrequencyMhz(40);

  Serial.begin(9600);
  while (!Serial);
  pr("");
  pr("Start!");

  //// Temperature/Humidity Sensor
  #if TEMPSENSOR == true
    DHT dht(DHTTYPE);
  #endif
  
  // Begin the Temperature/Humidity sensor
  #if TEMPSENSOR == true
    dht.begin();
  #endif

  #if TEMPSENSOR == true
    float temp_hum_val[2] = {0};
    if (!dht.readTempAndHumidity(temp_hum_val)) {
      // Default result is in Celcius
      temp_hum_val[1] = temp_hum_val[1];
    } else {
        temp_hum_val[0] = -666.0;
        temp_hum_val[1] = -666.0;
    }
    pr("Humidity:");
    pr(temp_hum_val[1]);
    pr("Temperature:");
    pr(temp_hum_val[0]);
  #endif
}

void loop() {

  
  
  pr("SENITEE CHEK SHOULD NOT PRINT.");
}