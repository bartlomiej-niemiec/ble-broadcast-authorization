#ifndef BLE_BROADCAST_SECURITY_ENCRYPTOR_H
#define BLE_BROADCAST_SECURITY_ENCRYPTOR_H

#include "beacon_pdu_key_reconstruction.h"
#include "beacon_pdu_data.h"
#include "crypto.h"
#include "stdbool.h"

typedef struct {
    key_splitted key_fragments;
    uint32_t session_id;
    uint8_t key_fragment_index;
    uint32_t time_interval_ms;
    uint32_t last_time_interval_ms;
} key_encryption_parameters;

typedef void (*key_fragment_encrypted_callback)(beacon_key_pdu * bkpdu);

void register_fragment_encrypted_callback(key_fragment_encrypted_callback callback);

bool init_and_start_up_encryptor_task();

void request_key_fragment_pdu_encryption(key_encryption_parameters * parameters);

#endif