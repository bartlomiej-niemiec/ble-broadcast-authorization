
#ifndef KEY_CACHE_HPP
#define KEY_CACHE_HPP

#include "crypto.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdint.h>

typedef struct {
    key_128b key;
    uint8_t key_id;
} key_reconstruction_map;

typedef struct {
    key_reconstruction_map* map;
    SemaphoreHandle_t xMutexCacheAccess;
    uint8_t cache_size;
} key_reconstruction_cache;

int create_key_cache(key_reconstruction_cache ** key_cache, const uint8_t cache_size);

int init_key_cache(key_reconstruction_cache * key_cache);

int add_key_to_cache(key_reconstruction_cache * const key_cache, key_128b * key, uint8_t key_id);

int remove_key_from_cache(key_reconstruction_cache * const key_cache, uint8_t key_id);

bool is_key_in_cache(key_reconstruction_cache * const key_cache, uint8_t key_id);

int get_key_from_cache(key_reconstruction_cache * const key_cache, key_128b * const reconstructed_key, uint8_t key_id);


#endif
