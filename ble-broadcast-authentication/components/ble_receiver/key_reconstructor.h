#ifndef KEY_RECONSTRUCTOR_H
#define KEY_RECONSTRUCTOR_H

#include "crypto.h"

void start_up_key_reconstructor(void);

int queue_key_for_reconstruction(uint16_t key_id, uint8_t key_fragment, uint8_t * encrypted_key_fragment, uint8_t * key_hmac, uint8_t xor_seed);

typedef void (*key_reconstruciton_cb)(uint8_t key_id, const key_128b * const reconstructed_key);

void register_callback_to_key_reconstruction(key_reconstruciton_cb cb);

#endif