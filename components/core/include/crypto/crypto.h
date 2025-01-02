#ifndef CRYPTO_H
#define CRYPTO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define KEY_FRAGMENT_SIZE 4
#define NO_KEY_FRAGMENTS 4
#define KEY_SIZE ((KEY_FRAGMENT_SIZE) * (NO_KEY_FRAGMENTS))
#define HMAC_SIZE 4
#define NONCE_SIZE 4
#define SESSION_ID_SIZE sizeof(uint32_t)

// AES Key and IV sizes
#define AES_KEY_SIZE 16
#define AES_IV_SIZE 16

typedef struct {
    uint8_t  fragment[NO_KEY_FRAGMENTS][KEY_FRAGMENT_SIZE];
} key_splitted;

typedef struct {
    uint8_t key[KEY_SIZE];
} key_128b;

void generate_128b_key(key_128b * key);

void split_128b_key_to_fragment(key_128b * key, key_splitted * key_splitted);

void get_128b_key_from_fragments(key_128b * key, key_splitted * key_splitted);

void add_fragment_to_key_spliited(key_splitted * key_splitted, uint8_t *fragment, uint8_t fragment_index);

bool encrypt_payload(const uint8_t *key, const uint8_t *payload, size_t payload_size, 
                     const uint8_t *session_id, const uint8_t *nonce, 
                     uint8_t *encrypted_payload);

bool decrypt_payload(const uint8_t *key, const uint8_t *encrypted_payload, size_t payload_size, 
                     const uint8_t *session_id, const uint8_t *nonce, 
                     uint8_t *decrypted_payload);

bool decrypt_key_fragment(const uint8_t *encrypted_fragment, size_t fragment_size, 
                          uint32_t prev_timestamp, uint32_t current_timestamp, 
                          uint32_t session_data, const uint8_t *hmac_received, 
                          uint8_t *decrypted_fragment);

void encrypt_key_fragment(const uint8_t *key_fragment, size_t fragment_size, 
                          uint32_t prev_timestamp, uint32_t current_timestamp, 
                          uint32_t session_data, uint8_t *encrypted_fragment, uint8_t *hmac);

uint8_t get_random_seed();

void fill_random_bytes(uint8_t *arr, size_t size_arr);

void calculate_hmac_of_fragment(uint8_t *encrypted_fragment, uint8_t *marker, uint8_t pdu_type, uint8_t *session_data, uint8_t *nonce, uint8_t *hmac_output);

int crypto_secure_memcmp(const void *a, const void *b, size_t size);

void build_key_frament_nonce(uint8_t *nonce, uint8_t nonce_size, uint32_t curr_timestamp, uint32_t prev_timestamp, uint16_t prev_next_arrival_time_ms, uint16_t curr_next_arrival_time_ms);

#endif