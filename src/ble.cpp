#include "ble.h"

#include <Arduino.h>
#include <esp_bt.h>               // ESP32 BLE
#include <esp_bt_device.h>        // ESP32 BLE
#include <esp_bt_main.h>          // ESP32 BLE
#include <esp_err.h>              // ESP32 ESP-IDF
#include <esp_gap_ble_api.h>      // ESP32 BLE
#include <esp_gatt_common_api.h>  // ESP32 BLE
#include <esp_gattc_api.h>        // ESP32 BLE
#include <esp_gatts_api.h>        // ESP32 BLE

namespace Ble {
DataElement::DataElement(const void *valuePtr, size_t size) : valueSize(size), value(0) {
  this->value = malloc(valueSize);
  memcpy(this->value, valuePtr, valueSize);
}
DataElement::DataElement(uint32_t value) : DataElement((void *)&value, sizeof(value)) {
}
DataElement::DataElement(int32_t value) : DataElement((void *)&value, sizeof(value)) {
}
DataElement::DataElement(float_t value) : DataElement((void *)&value, sizeof(value)) {
}

size_t getSize(DataElements dataElements) {
  size_t size = 0;
  for (int i = 0; i < dataElements.size(); i++) {
    DataElement de = dataElements.at(i);
    size += de.valueSize;
  }
  return size;
}

void printHex(uint8_t *buf, size_t size) {
  char str[32 * 2 + 1];
  size = (size > 32) ? (32) : (size);
  unsigned char *pin = buf;
  const char *hex = "0123456789abcdef";
  char *pout = str;
  for (; pin < buf + size; pout += 2, pin++) {
    pout[0] = hex[(*pin >> 4) & 0xF];
    pout[1] = hex[*pin & 0xF];
  }
  pout[0] = 0;
  log_v("hex: %s", str);
}

void serialize(DataElements dataElements, uint8_t *dst) {
  for (int i = 0; i < dataElements.size(); i++) {
    DataElement de = dataElements.at(i);
    memcpy(dst, de.value, de.valueSize);
    dst = dst + de.valueSize;
  }
}

bool advertise(DataElements dataElements) {
  // esp_ble_adv_data_t advData;
  esp_ble_adv_params_t advParams;
  esp_err_t errRc = ESP_OK;

  if (!btStart()) {
    log_e("btStart: nok");
    return false;
  } else {
    esp_bluedroid_status_t bt_state = esp_bluedroid_get_status();
    log_v("btStart: ok - esp_bluedroid_get_status: %d\n", bt_state);
    if (bt_state == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
      errRc = esp_bluedroid_init();
      if (errRc != ESP_OK) {
        log_e("esp_bluedroid_init: %d\n", errRc);
        return false;
      }
    }

    if (bt_state != ESP_BLUEDROID_STATUS_ENABLED) {
      errRc = esp_bluedroid_enable();
      if (errRc != ESP_OK) {
        log_e("esp_bluedroid_enable: %d\n", errRc);
        return false;
      }
    }
  }

  // advData.set_scan_rsp = false;
  // advData.include_name = true;
  // advData.include_txpower = true;
  // advData.min_interval = 0x20;
  // advData.max_interval = 0x40;
  // advData.appearance = 0x00;
  // advData.manufacturer_len = 0;
  // advData.p_manufacturer_data = nullptr;
  // advData.service_data_len = 0;
  // advData.p_service_data = nullptr;
  // advData.service_uuid_len = 0;
  // advData.p_service_uuid = nullptr;
  // advData.flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

  advParams.adv_int_min = 0x20;
  advParams.adv_int_max = 0x40;
  advParams.adv_type = ADV_TYPE_IND;
  advParams.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  advParams.channel_map = ADV_CHNL_ALL;
  advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
  advParams.peer_addr_type = BLE_ADDR_TYPE_PUBLIC;

  size_t dataSize = getSize(dataElements);
  uint8_t *raw_adv_data = (uint8_t *)malloc(dataSize + 4);
  raw_adv_data[0] = dataSize + 3;                           // data size + adv type field
  raw_adv_data[1] = ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE;  // set adv type
  raw_adv_data[2] = 0xFF;                                   // Custom manufacturer
  raw_adv_data[3] = 0xFF;                                   // Custom manufacturer
  serialize(dataElements, &raw_adv_data[4]);
  // printHex(raw_adv_data, dataSize + 4);

  errRc = esp_ble_gap_config_adv_data_raw(raw_adv_data, dataSize + 4);
  if (errRc != ESP_OK) {
    log_e("esp_ble_gap_config_adv_data_raw: %d\n", errRc);
  }
  free(raw_adv_data);

  errRc = esp_ble_gap_start_advertising(&advParams);
  if (errRc != ESP_OK) {
    log_e("esp_ble_gap_start_advertising: %d\n", errRc);
  }

  return true;
}

}  // namespace Ble
