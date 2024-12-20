#ifndef BLE_CONSUMER_COLLECTION_H
#define BLE_CONSUMER_COLLECTION_H

#include "stdint.h"
#include "freertos/FreeRTOS.h"
#include "ble_consumer.h"

#define MAX_BLE_CONSUMERS 5

typedef struct {
    uint8_t size;
    ble_consumer *arr;
    uint8_t consumers_count;
    SemaphoreHandle_t xMutex;
} ble_consumer_collection;


ble_consumer_collection * create_ble_consumer_collection(const uint8_t collection_size, const uint8_t sender_key_cache_size);

int add_consumer_to_collection(ble_consumer_collection * p_ble_consumer_collection, esp_bd_addr_t mac_address_arr);

ble_consumer * get_ble_consumer_from_collection(ble_consumer_collection * p_ble_consumer_collection, esp_bd_addr_t mac_address_arr);

int remove_lru_consumer_from_collection(ble_consumer_collection * p_ble_consumer_collection);

int remove_consumer_from_collection(ble_consumer_collection * p_ble_consumer_collection, esp_bd_addr_t mac_address_arr);

int get_active_no_consumers(ble_consumer_collection * p_ble_consumer_collection);

#endif