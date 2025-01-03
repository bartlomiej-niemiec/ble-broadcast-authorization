#ifndef BEACON_PDU_KEY_RECONSTRUCTION_DATA_H
#define BEACON_PDU_KEY_RECONSTRUCTION_DATA_H

#include <esp_err.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "beacon_pdu/beacon_key_data.h"
#include "beacon_pdu/beacon_marker.h"

typedef struct {
    beacon_marker marker;
    uint8_t type;
    beacon_key_data key_data;
}__attribute__((packed)) beacon_key_pdu;

#endif