#ifndef CRYPTO_H
#define CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#define KEY_FRAGMENT_SIZE 4
#define NO_KEY_FRAGMENTS 4

typedef struct {
    uint8_t  fragment[NO_KEY_FRAGMENTS][KEY_FRAGMENT_SIZE];
} key_splitted;

typedef struct {
    uint8_t key[KEY_FRAGMENT_SIZE * NO_KEY_FRAGMENTS];
} key_128b;

void generate_128b_key(key_128b * key);

void split_128b_key_to_fragment(key_128b * key, key_splitted * key_splitted);

void get_128b_key_from_fragments(key_128b * key, key_splitted * key_splitted);

uint16_t calculate_crc_for_key_fragment(key_splitted * key_splitted, uint8_t no_fragment);

void add_fragment_to_key_spliited(key_splitted * key_splitted, uint8_t *fragment, uint8_t fragment_index);

void generate_iv(uint16_t pdu_id, uint64_t time_interval, uint8_t *iv);

int decrypt_payload_aes_ctr(uint8_t *encrypted_payload, size_t payload_size, uint8_t *key, uint8_t *decrypted_payload, uint8_t *iv);

int encrypt_payload_aes_ctr(uint8_t *payload, size_t payload_size, uint8_t *key, uint8_t *encrypted_payload, uint8_t *iv);

#endif