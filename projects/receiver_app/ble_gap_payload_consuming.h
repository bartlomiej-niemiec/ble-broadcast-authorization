#ifndef BLE_SEC_PAYLOAD_CONSUMER_H
#define BLE_SEC_PAYLOAD_CONSUMER_H

#include "esp_gap_ble_api.h"
#include <stdint.h>

void payload_notifier(uint8_t *data, size_t data_size, esp_bd_addr_t mac_address);

void receiver_app_scan_complete_cb(int64_t timestamp_us, uint8_t *data, size_t data_size, esp_bd_addr_t mac_address);

#endif