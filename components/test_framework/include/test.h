#ifndef TEST_H
#define TEST_H

#include "esp_gap_ble_api.h"
#include "stdint.h"

#define TEST_ESP_LOG_GROUP "TEST_LOG_GROUP"

void init_test();

void start_test_measurment();

void end_test_measurment();

void packet_received_cb(uint8_t *data, size_t data_len, esp_bd_addr_t mac_address);

void packet_send_cb(uint8_t *data, size_t data_len, esp_bd_addr_t mac_address);

void key_reconstruction_start(esp_bd_addr_t mac_address, uint16_t key_id);

void key_reconstruction_end(esp_bd_addr_t mac_address, uint16_t key_id);

void deferred_queue_percentage(double percentage, esp_bd_addr_t mac_address);

void processing_queue_percentage(double percentage);

#endif