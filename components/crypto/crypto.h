#ifndef CRYPTO_H
#define CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#define KEY_FRAGMENT_SIZE 4
#define NO_KEY_FRAGMENTS 4
#define KEY_SIZE ((KEY_FRAGMENT_SIZE) * (NO_KEY_FRAGMENTS))
#define HMAC_SIZE 4

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

int aes_ctr_encrypt_payload(uint8_t *input, size_t length, uint8_t *key, uint8_t *nonce, uint8_t *output);

int aes_ctr_decrypt_payload(uint8_t *input, size_t length, uint8_t *key, uint8_t *nonce, uint8_t *output);

void xor_encrypt_key_fragment(uint8_t  fragment[KEY_FRAGMENT_SIZE], uint8_t  encrypted_fragment[KEY_FRAGMENT_SIZE], uint8_t xor_seed);

void xor_decrypt_key_fragment(uint8_t  encrypted_fragment[KEY_FRAGMENT_SIZE], uint8_t  decrypted_fragment[KEY_FRAGMENT_SIZE], uint8_t xor_seed);

uint8_t get_random_seed();

void calculate_hmac_of_fragment(uint8_t *key_fragment, uint8_t *encrypted_fragment, uint8_t *hmac_output);

int crypto_secure_memcmp(const void *a, const void *b, size_t size);

#endif