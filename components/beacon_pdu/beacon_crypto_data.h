#ifndef BEACON_CRYPTO_DATA_H
#define BEACON_CRYPTO_DATA_H

#include <stdint.h>
#include "crypto.h"



typedef struct {            
    uint16_t key_id;                                // unique ID for Symmetric Key
    uint8_t key_fragment_no;                        // number of key fragment (0 - 3)
    uint8_t enc_key_fragment[KEY_FRAGMENT_SIZE];    // encrypted key fragment
    uint8_t xor_seed;                               // random seed for key fragment xor encryption
    uint8_t key_fragment_hmac[HMAC_SIZE];           // hmac of key fragment
} __attribute__((packed)) beacon_crypto_data;

#define CRYPT_DATA_SIZE sizeof(beacon_crypto_data)

#endif