#ifndef BEACON_PDU_DATA_H
#define BEACON_PDU_DATA_H

#include <esp_err.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "beacon_crypto_data.h"
#include "beacon_marker.h"

#define MAX_GAP_DATA_LEN 29
#define MAX_PDU_PAYLOAD_SIZE (MAX_GAP_DATA_LEN - (CRYPT_DATA_SIZE) - (MARKER_STRUCT_SIZE))

typedef struct {
    beacon_marker marker;
    beacon_crypto_data bcd;
    uint8_t payload[MAX_PDU_PAYLOAD_SIZE];
}__attribute__((packed)) beacon_pdu_data;

esp_err_t build_beacon_pdu_data (beacon_crypto_data *crypto, uint8_t* payload, size_t payload_size, beacon_pdu_data *bpd);

esp_err_t fill_marker_in_pdu(beacon_pdu_data *bpd);

bool is_pdu_in_beacon_pdu_format(uint8_t *data, size_t size);

#endif