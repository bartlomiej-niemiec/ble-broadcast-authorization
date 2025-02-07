#ifndef BEACON_CRYPTO_DATA_H
#define BEACON_CRYPTO_DATA_H

#include <stdint.h>
#include "crypto/crypto.h"

#define PDU_INTERVAL_RESOLUTION_MS 50
#define MAX_TIME_INTERVAL_MULTIPLIER 16
#define KEY_EXCHANGE_COUNTER_TARGET 15

typedef struct {            
    uint16_t key_session_data;                    // unique ID for Symmetric Key
    uint8_t enc_key_fragment[KEY_FRAGMENT_SIZE];  // encrypted key fragment
    uint8_t xor_seed;                             // random seed for key fragment xor encryption
    uint8_t key_fragment_hmac[HMAC_SIZE];         // hmac of key fragment
} __attribute__((packed)) beacon_crypto_data;

#define CRYPT_DATA_SIZE sizeof(beacon_crypto_data)

#endif