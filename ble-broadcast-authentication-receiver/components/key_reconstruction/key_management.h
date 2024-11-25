#ifndef KEY_MANEGEMENT_H
#define KEY_MANEGEMENT_H

#include <stdbool.h>
#include <stdint.h>
#include "key_fragmented.h"

#define KEY_COLLECTIONS_SIZE 3

typedef struct {
    uint16_t decrypted_key_fragment;
    uint16_t key_fragment_id;
} encrypted_key_fragmend;

typedef struct{
    encrypted_key_fragmend key_fragments[NO_KEY_FRAGMENTS];
    uint8_t key_id;
    int no_collected_key_fragments;
    uint32_t key;
} key_management;

void init_key_management(key_management* km);

int reconstruct_key_from_key_fragments(key_management* km);

bool is_key_in_collection(uint8_t key_id);

bool add_new_key_to_collection(uint8_t key_id);

void add_fragment_to_key_management(uint8_t key_id, uint16_t key_fragment, uint16_t key_fragment_id);

bool is_key_available(uint8_t key_id);

#endif