#ifndef KEY_MANAGEMENT_H
#define KEY_MANAGEMENT_H

#include <stdbool.h>
#include <stdint.h>
#include "crypto.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct{
    key_splitted key_fragments;
    uint8_t key_id;
    int no_collected_key_fragments;
    bool decrypted_key_fragments[KEY_FRAGMENT_SIZE];
    key_128b key;
} key_management;

typedef struct {
    key_management * km;
    size_t key_management_size;
    SemaphoreHandle_t xMutex;
} key_reconstruction_collection;

key_reconstruction_collection* create_new_key_collection(size_t key_collection_size);

void remove_key_from_collection(key_reconstruction_collection* key_collection, uint8_t key_id);

bool reconstruct_key_from_key_fragments(key_reconstruction_collection* key_collection, key_128b* km, uint8_t key_id);

bool is_key_in_collection(key_reconstruction_collection* key_collection, uint8_t key_id);

bool add_new_key_to_collection(key_reconstruction_collection* key_collection, uint8_t key_id);

void add_fragment_to_key_management(key_reconstruction_collection* key_collection, uint8_t key_id, uint8_t *fragment, uint8_t key_fragment_id);

bool is_key_available(key_reconstruction_collection* key_collection, uint8_t key_id);

bool is_key_fragment_decrypted(key_reconstruction_collection* key_collection, uint8_t key_id, uint8_t key_fragment);

#endif