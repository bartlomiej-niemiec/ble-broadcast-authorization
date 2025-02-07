#ifndef BEACON_PDU_DATA_H
#define BEACON_PDU_DATA_H

#include <esp_err.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "beacon_crypto_data.h"
#include "beacon_marker.h"

#define MAX_GAP_DATA_LEN 31
#define MAX_PDU_PAYLOAD_SIZE (MAX_GAP_DATA_LEN - (CRYPT_DATA_SIZE) - (MARKER_STRUCT_SIZE))
#define NONCE_SIZE 16

typedef struct {
    beacon_marker marker;
    beacon_crypto_data bcd;
    uint8_t payload[MAX_PDU_PAYLOAD_SIZE];
    size_t payload_size;
}__attribute__((packed)) beacon_pdu_data;

size_t get_beacon_pdu_data_len(beacon_pdu_data * pdu);

size_t get_payload_size_from_pdu(size_t total_pdu_len);

esp_err_t build_beacon_pdu_data (beacon_crypto_data *crypto, uint8_t* payload, size_t payload_size, beacon_pdu_data *bpd);

esp_err_t fill_marker_in_pdu(beacon_pdu_data *bpd);

bool is_pdu_in_beacon_pdu_format(uint8_t *data, size_t size);

void build_nonce(uint8_t nonce[NONCE_SIZE], const beacon_marker* marker, uint16_t key_session_data, uint8_t xor_seed);

uint16_t produce_key_session_data(uint16_t key_id, uint8_t key_fragment);

uint8_t produce_key_exchange_data(uint8_t pdu_time_interval_ms, uint8_t key_exchange_counter);

uint16_t get_key_id_from_key_session_data(uint16_t session_data);

uint8_t get_key_fragment_index_from_key_session_data(uint16_t session_data);

uint16_t get_key_expected_time_interval_ms(uint8_t key_exchange_data);

uint8_t get_key_exchange_counter(uint8_t key_exchange_data);

uint8_t get_key_expected_time_interval_multiplier(uint8_t key_exchange_data);

#endif