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
float analogToSensorHeight(uint16_t x) {
  float a = 0;
  float b = 0;
  if (x > 1903) {
    // 25.4831 - 0.0120773 x
    a = -0.0120773;
    b = 25.4831;
  } else if (x > 1438) {
    // 47.5172 - 0.0236559 x
    a = -0.0236559;
    b = 47.5172;
  } else if (x > 1025) {
    // 57.023 - 0.0302663 x
    a = -0.0302663;
    b = 57.023;
  } else if (x > 651) {
    // 94.516 - 0.0668449 x
    a = -0.0668449;
    b = 94.516;
  } else if (x > 450) {
    // 106.06 - 0.0845771 x
    a = -0.0845771;
    b = 106.06;
  } else {
    // 97.25 - 0.065 x
    a = -0.065;
    b = 97.25;
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

RTC_DATA_ATTR uint16_t lastSentRawFillingLevel;
RTC_DATA_ATTR uint16_t lastSentRawBatteryStatus;

void setup() {
#if DCORE_DEBUG_LEVEL >= 5
  Serial.begin(115200);
  // wait for serial port to connect. Needed for native USB
  while (!Serial) {
  }
  print_wakeup_reason();  // Print the wakeup reason for ESP32
#endif

  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);
  analogSetAttenuation(ADC_0db);
  digitalWrite(D3, LOW);  // Spannungsteiler wird über D3 versorgt.
  digitalWrite(D4, LOW);  // Spannungsteiler wird über D3 versorgt.

  uint16_t rawFillingLevel = adcSample(A0, D3, 32);
  uint16_t rawBatteryStatus = adcSample(A3, D4, 32);

  if ((abs(int32_t(lastSentRawFillingLevel) - int32_t(rawFillingLevel)) > 5) ||
      (abs(int32_t(lastSentRawBatteryStatus) - int32_t(rawBatteryStatus)) > 5)) {
    // if (true) {
    lastSentRawFillingLevel = rawFillingLevel;
    lastSentRawBatteryStatus = rawBatteryStatus;
    float fillingLevel = analogToSensorHeight(rawFillingLevel);
    log_v("fillingLevel (raw - height): %d - %f", rawFillingLevel, fillingLevel);
    log_v("batteryStatus (raw)         : %f", rawBatteryStatus);
    Ble::DataElements dataElements = {
        Ble::DataElement(rawFillingLevel),
        Ble::DataElement(fillingLevel),
        Ble::DataElement(rawBatteryStatus)};

    Ble::advertise(dataElements);
    delay(100);
  }
  deepSleep(30);
}

void loop() {
  // printSensorHeight();
  // printBatteryVoltage();
  // Ble::loop();
  // delay(5000);
  // deepSleep(5);
}