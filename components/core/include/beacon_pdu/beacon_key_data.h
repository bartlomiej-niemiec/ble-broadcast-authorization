#ifndef BEACON_CRYPTO_DATA_H
#define BEACON_CRYPTO_DATA_H

#include <stdint.h>
#include "crypto/crypto.h"

#define TIME_INTERVAL_RESOLUTION_MS 50
#define MAX_TIME_INTERVAL_MS 500

typedef struct {            
    uint32_t session_data;                          // unique ID for Symmetric Key and session key fragment
    uint8_t nonce[NONCE_SIZE];
    uint8_t enc_key_fragment[KEY_FRAGMENT_SIZE];    // encrypted key fragment                          // random seed for key fragment xor encryption
    uint8_t key_fragment_hmac[HMAC_SIZE];           // hmac of key fragment
} __attribute__((packed)) beacon_key_data;

#define PDU_KEY_DATA_SIZE sizeof(beacon_key_data)

#endif