#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_gap_ble_api.h"

#define MAX_BLE_BROADCAST_SIZE_BYTES 31

typedef struct{
    uint8_t data[MAX_BLE_BROADCAST_SIZE_BYTES];
    size_t data_len;
    esp_bd_addr_t mac_address;
} ble_broadcast_pdu;

void dispatch_pdu(ble_broadcast_pdu* pdu, esp_bd_addr_t mac_address);

bool create_ble_broadcast_pdu_for_dispatcher(ble_broadcast_pdu* pdu, uint8_t *data, size_t size, esp_bd_addr_t mac_address);

#endif