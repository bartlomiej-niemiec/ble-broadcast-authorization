#ifndef KEY_RECONSTRUCTOR_H
#define KEY_RECONSTRUCTOR_H

#include "crypto.h"
#include "esp_gap_ble_api.h"

typedef enum{
    QUEUED_SUCCESS,
    QUEEUD_FAILED_NOT_INITIALIZED,
    QUEUED_FAILED_NO_SPACE,
    QUEUED_FAILED_BAD_INPUT,
    QUEUED_FAILED_KEY_ALREADY_RECONSTRUCTED
} RECONSTRUCTION_QUEUEING_STATUS;

int start_up_key_reconstructor(const uint8_t max_key_reconstrunction_count);

RECONSTRUCTION_QUEUEING_STATUS queue_key_for_reconstruction(uint16_t key_id, uint8_t key_fragment_no, uint8_t * encrypted_key_fragment, uint8_t * key_hmac, uint8_t xor_seed, const esp_bd_addr_t consumer_mac_address);

typedef void (*key_reconstruction_complete_cb)(uint8_t key_id, key_128b * const reconstructed_key, esp_bd_addr_t consumer_mac_address);

void register_callback_to_key_reconstruction(key_reconstruction_complete_cb cb);

#endif