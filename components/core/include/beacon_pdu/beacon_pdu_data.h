#ifndef BEACON_PDU_DATA_H
#define BEACON_PDU_DATA_H

#include <esp_err.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "beacon_marker.h"
#include "crypto.h"

#define MAX_GAP_DATA_LEN 31
#define MAX_PDU_PAYLOAD_SIZE (MAX_GAP_DATA_LEN - (MARKER_STRUCT_SIZE + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(NONCE_SIZE * sizeof(uint8_t))))

typedef struct {
    beacon_marker marker;
    uint8_t type;
    uint32_t session_id;
    uint8_t nonce[NONCE_SIZE];
    uint8_t payload[MAX_PDU_PAYLOAD_SIZE];
}__attribute__((packed)) beacon_pdu_data;

#endif