#ifndef BEACON_CRYPTO_DATA_H
#define BEACON_CRYPTO_DATA_H

#include <stdint.h>

typedef struct {
    uint16_t pdu_id;             // unique ID of PDU
    uint16_t key_id;             // unique ID for Symmetric Key
    uint8_t key_fragment_no;    // number of key fragment (1 - 3)
    uint8_t enc_key_fragment[4];   // encrypted key fragment
    uint16_t key_fragment_crc;   // crc of key fragment
} __attribute__((packed)) beacon_crypto_data;

#define CRYPT_DATA_SIZE sizeof(beacon_crypto_data)

#endif