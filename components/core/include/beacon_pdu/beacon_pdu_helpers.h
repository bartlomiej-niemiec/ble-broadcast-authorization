#ifndef BEACON_PDU_HELPERS_H
#define BEACON_PDU_HELPERS_H

#include <esp_err.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "beacon_pdu/beacon_marker.h"
#include "beacon_pdu/beacon_pdu_data.h"
#include "beacon_pdu/beacon_pdu_key_reconstruction.h"

typedef enum {
    PDU_KEY_RECONSTRUCTION,
    PDU_PAYLOAD
} PDU_TYPES;

esp_err_t build_beacon_pdu_data (uint8_t* payload, size_t payload_size, uint8_t* session_id, uint8_t session_id_size, uint8_t nonce[NONCE_SIZE], beacon_pdu_data *bpd);

esp_err_t build_beacon_pdu_key (beacon_key_data* key_data, beacon_key_pdu *bpd);

esp_err_t fill_marker_in_pdu(beacon_pdu_data *bpd);

bool is_pdu_in_beacon_pdu_format(uint8_t *data, size_t size);

uint32_t encode_session_data(uint32_t session_id, uint8_t key_fragment);

void decode_session_fragment(uint32_t encoded, uint32_t *session_id, uint8_t *key_fragment);

#endif