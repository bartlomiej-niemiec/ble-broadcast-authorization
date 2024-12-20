#ifndef BLE_CONSUMER_H
#define BLE_CONSUMER_H

#include "esp_gap_ble_api.h"
#include "freertos/FreeRTOS.h"
#include "stdint.h"
#include "key_cache.h"
#include "key_reconstructor.h"

#define DEFERRED_QUEUE_SIZE 30
#define KEY_CACHE_SIZE 2

typedef struct {
    QueueHandle_t deferredQueue;
    key_reconstruction_cache *key_cache;
    uint8_t key_cache_size;
    int16_t recently_removed_key_id;
    uint8_t deferred_queue_count;
} ble_consumer_context;


typedef struct {
    ble_consumer_context context;
    esp_bd_addr_t mac_address_arr;
    uint64_t last_pdu_timestamp;
    uint8_t rollover;
} ble_consumer;


ble_consumer * create_ble_consumer(const uint8_t key_cache_size);

int create_ble_consumer_resources(ble_consumer *const p_ble_consumer, const uint8_t key_cache_size);

int destroy_ble_consumer(ble_consumer *p_ble_consumer);

int init_ble_consumer(ble_consumer *p_ble_consumer);

int reset_ble_consumer(ble_consumer * p_ble_consumer);

#endif