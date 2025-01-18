#ifndef TEST_H
#define TEST_H

#include "esp_gap_ble_api.h"
#include "stdint.h"

#define TEST_ESP_LOG_GROUP "TEST_LOG_GROUP"

extern uint8_t test_payload_4_bytes[4];
extern uint8_t test_payload_10_bytes[10];
extern uint8_t test_payload_16_bytes[16];

typedef enum {
    TEST_SENDER_ROLE,
    TEST_RECEIVER_ROLE
} TEST_ROLE;

void init_test();

void start_test_measurment(TEST_ROLE role);

void end_test_measurment();

void test_log_sender_data(size_t payload_size, int adv_interval);

void test_log_sender_key_replace_time_in_s(uint16_t key_replace_time_in_s);

void test_log_packet_received(uint8_t *data, size_t data_len, esp_bd_addr_t mac_address);

void test_log_packet_send(uint8_t *data, size_t data_len, esp_bd_addr_t mac_address);

void test_log_key_reconstruction_start(esp_bd_addr_t mac_address, uint16_t key_id);

void test_log_key_reconstruction_end(esp_bd_addr_t mac_address, uint16_t key_id);

void test_log_deferred_queue_percentage(double percentage, esp_bd_addr_t mac_address);

void test_log_processing_queue_percentage(double percentage);

bool is_pdu_from_expected_sender(esp_bd_addr_t addr);

void test_log_bad_structure_packet(esp_bd_addr_t addr);

#endif