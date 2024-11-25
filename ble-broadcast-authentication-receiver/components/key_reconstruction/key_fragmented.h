#ifndef KEY_FRAGMENTED_H
#define KEY_FRAGMENTED_H

#include <stdint.h>

#define NO_KEY_FRAGMENTS 3

typedef struct{
    uint16_t encypted_key_fragment;
    uint16_t pdu_id;
    uint16_t time_interval_ms;
    uint16_t crc;
} key_fragment;

void decrypt_key_fragment(key_fragment * kf, uint16_t * decrypted_key_fragment);

bool is_key_fragment_decrypted_correct(uint16_t * decrypted_key_fragment, uint16_t crc);

bool crete_new_key_fragment(key_fragment *kf, uint16_t encypted_key_fragment, uint16_t pdu_id, uint16_t time_interval_ms, uint16_t crc); 

#endif