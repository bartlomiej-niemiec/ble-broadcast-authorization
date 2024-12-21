#ifndef BLE_SEC_PAYLOAD_CONSUMER_H
#define BLE_SEC_PAYLOAD_CONSUMER_H

#include "esp_gap_ble_api.h"
#include <stdint.h>

void start_up_payload_consuming_task();

void payload_notifier(uint8_t *data, size_t data_len, esp_bd_addr_t mac_address);


#endif