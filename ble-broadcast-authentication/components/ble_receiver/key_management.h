#ifndef KEY_MANEGEMENT_H
#define KEY_MANEGEMENT_H

#include <stdbool.h>
#include <stdint.h>
#include "crypto.h"

#define KEY_COLLECTIONS_SIZE 3

typedef struct{
    key_splitted key_fragments;
    uint8_t key_id;
    int no_collected_key_fragments;
    bool decrypted_key_fragments[KEY_FRAGMENT_SIZE];
    key_128b key;
} key_management;

void remove_key_from_collection(uint8_t key_id);

bool reconstruct_key_from_key_fragments(key_128b* km, uint8_t key_id);

bool is_key_in_collection(uint8_t key_id);

bool add_new_key_to_collection(uint8_t key_id);

void add_fragment_to_key_management(uint8_t key_id, uint8_t *fragment, uint8_t key_fragment_id);

bool is_key_available(uint8_t key_id);

bool is_key_fragment_decrypted(uint8_t key_id, uint8_t key_fragment);

#endif