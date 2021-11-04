#include <Arduino.h>

#include "ble.h"
#include "esp32-hal-log.h"

// Prints the reason for wakeup
void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      log_v("Wakeup caused by external signal using RTC_IO");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      log_v("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      log_v("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      log_v("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      log_v("Wakeup caused by ULP program");
      break;
    default:
      log_v("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
      break;
  }
}

void deepSleep(uint32_t secondsToSleep) {
  // Convert time before wakeup to microseconds
  esp_sleep_enable_timer_wakeup(secondsToSleep * 1000000);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
#if DCORE_DEBUG_LEVEL >= 5
  log_v("Going to deep-sleep in 5 sec for %d sec\n", secondsToSleep);
  Serial.flush();
#endif
  esp_deep_sleep_start();
}

// adcSample samples given ADC `samples` times and returns the average.
// `dOutPin` is pulled to HIGH while doing so (OUTPUT pin mode must
// have been set before).
uint16_t adcSample(uint8_t aInPin, uint8_t dOutPin, uint16_t samples) {
  uint32_t avg = 0;
  digitalWrite(dOutPin, HIGH);
  for (int i = 0; i < samples; i++) {
    uint16_t a = analogRead(aInPin);
    avg = avg + a;
  }
  digitalWrite(dOutPin, LOW);
  return (avg / samples);
}

// analogToSensorHeight converts `x` (raw adc reading) to level height.
float analogToSensorHeight(float x) {
  float a = 0;
  float b = 0;
  if (x > 0.595) {
    // 26.1111 - 39.6825 x
    a = -39.6825;
    b = 26.1111;
  } else if (x > 0.448) {
    // 46.619 - 74.1497 x
    a = -74.1497;
    b = 46.619;
  } else if (x > 0.316) {
    // 55.8242 - 94.697 x
    a = -94.697;
    b = 55.8242;
  } else if (x > 0.198) {
    // 93.6525 - 214.407 x
    a = -214.407;
    b = 93.6525;
  } else if (x > 0.135) {
    // 104.314 - 268.254 x
    a = -268.254;
    b = 104.314;
  } else {
    // 95.7622 - 204.905 x
    a = -204.905;
    b = 95.7622;
  }
  return a * x + b;
}

uint16_t a[3] = {0, 0, 0};
bool print = false;

void printSensorHeight() {
  a[0] = adcSample(A0, D3, 32);
  float h = analogToSensorHeight(a[0]);
  log_v("****** %d - %f\n", a[0], h);
}

void printBatteryVoltage() {
  uint16_t batteryRaw = adcSample(A3, D4, 32);
  log_v("bat raw: %d", batteryRaw);
}

RTC_DATA_ATTR uint8_t wakeupCounter = 0;
RTC_DATA_ATTR uint8_t txCounter = 0;
RTC_DATA_ATTR uint16_t lastSentRawFillingLevel;
RTC_DATA_ATTR uint16_t lastSentRawBatteryStatus;

void setup() {
#if DCORE_DEBUG_LEVEL >= 5
  Serial.begin(115200);
  // wait for serial port to connect. Needed for native USB
  while (!Serial) {
  }
  print_wakeup_reason();  // Print the wakeup reason for ESP32
  log_v("Serial enabled");
#endif

  uint32_t sleepSeconds = 30;

  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);
  analogSetAttenuation(ADC_0db);
  digitalWrite(D3, LOW);  // Spannungsteiler wird über D3 versorgt.
  digitalWrite(D4, LOW);  // Spannungsteiler wird über D3 versorgt.

  uint16_t rawFillingLevel = adcSample(A0, D3, 32);
  uint16_t rawBatteryStatus = adcSample(A3, D4, 32);
  float normalisedFillingLevelRaw = (float)rawFillingLevel / (float)rawBatteryStatus;
  float fillingLevel = analogToSensorHeight(normalisedFillingLevelRaw);
  if (fillingLevel < 0.0) {
    fillingLevel = 0.0;
  } else if (fillingLevel > 94.0) {
    fillingLevel = 94.0;
  }
  if ((wakeupCounter % 8) == 0) {
    txCounter = 1;
  }
  if (fillingLevel > 0.5) {
    sleepSeconds = 10;
    txCounter = 5;
  }

  // if ((abs(int32_t(lastSentRawFillingLevel) - int32_t(rawFillingLevel)) > 5) ||
  //     (abs(int32_t(lastSentRawBatteryStatus) - int32_t(rawBatteryStatus)) > 5)) {
  if (txCounter > 0) {
    // if (true) {
    txCounter--;
    lastSentRawFillingLevel = rawFillingLevel;
    lastSentRawBatteryStatus = rawBatteryStatus;

    float fillingLevel = analogToSensorHeight(normalisedFillingLevelRaw);
    if (fillingLevel < 0.0) {
      fillingLevel = 0.0;
    } else if (fillingLevel > 94.0) {
      fillingLevel = 94.0;
    }
    log_v("fillingLevel (raw - height)     : %d - %f", rawFillingLevel, fillingLevel);
    log_v("batteryStatus (raw)             : %d", rawBatteryStatus);
    log_v("normalisedFillingLevelRaw (raw) : %f", normalisedFillingLevelRaw);
    Ble::DataElements dataElements = {
        Ble::DataElement(wakeupCounter),
        Ble::DataElement(rawFillingLevel),
        Ble::DataElement(fillingLevel),
        Ble::DataElement(rawBatteryStatus),
        Ble::DataElement(normalisedFillingLevelRaw)};

    Ble::advertise(dataElements);
    delay(1000);
  }
  wakeupCounter++;
  deepSleep(sleepSeconds);
}

void loop() {
  // printSensorHeight();
  // printBatteryVoltage();
  // Ble::loop();
  // delay(5000);
  // deepSleep(5);
}