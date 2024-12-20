#include "key_cache.h"
#include "esp_log.h"
#include "string.h"
#include "freertos/task.h"


static const char *KEY_CACHE_LOG_GROUP = "KEY CACHE LOG";

void save_tick_count_for_key_map(key_reconstruction_map * map)
{
    if (map != NULL)
    {
        uint32_t tickCount = xTaskGetTickCount();
        if (tickCount < map->last_used_timestamp)
        {
            map->rollover++;
        }
        map->last_used_timestamp = tickCount;
    }
}

uint64_t get_combined_timestamp(const key_reconstruction_map * map)
{
    return ((uint64_t)map->rollover << 32) | map->last_used_timestamp;
}

void reset_tick_count_for_map(key_reconstruction_map * map)
{
    if (map != NULL)
    {
        map->rollover = 0;
        map->last_used_timestamp = 0;
    }
}

int create_key_cache(key_reconstruction_cache ** key_cache, const uint8_t cache_size)
{
    int status = 0;
    *key_cache = (key_reconstruction_cache *) malloc(sizeof(key_reconstruction_cache));
    if (*key_cache != NULL)
    {
        ESP_LOGI(KEY_CACHE_LOG_GROUP, "Successfully allocated space for key cache at: %p", key_cache);
        (*key_cache)->map = (key_reconstruction_map* ) malloc(sizeof(key_reconstruction_map) * cache_size);
        (*key_cache)->cache_size = cache_size;
        (*key_cache)->last_key_id_used = -1;
        (*key_cache)->last_key_index_in_map = -1;
        (*key_cache)->xMutexCacheAccess = NULL;
        
        if ((*key_cache)->map == NULL)
        {
            ESP_LOGE(KEY_CACHE_LOG_GROUP, "Failed to allocate space for key cache map");
            free(*key_cache); // Free the main structure to avoid memory leaks
            *key_cache = NULL; // Reset the pointer
            status = -1;
        }
        else
        {
            ESP_LOGI(KEY_CACHE_LOG_GROUP, "Successfully allocated space for key cache map at: %p", (*key_cache)->map);
        }
    }
    else
    {
        status = -1;
    }

    return status;
}

int destroy_key_cache(key_reconstruction_cache * const key_cache)
{
    int status = 0;
    if (key_cache == NULL)
    {
        status = -1;
        return status;
    }
    else
    {
        vSemaphoreDelete(key_cache->xMutexCacheAccess);
        for (int i = 0; i < key_cache->cache_size; i++)
        {
            free(&(key_cache->map[i]));
        }
        free(key_cache);
    }
    return status;
}

int init_key_cache(key_reconstruction_cache * key_cache)
{
    int status = 0;

    if (key_cache == NULL)
    {
        return -3;
    }

    key_cache->xMutexCacheAccess = xSemaphoreCreateMutex();

    if (key_cache->xMutexCacheAccess == NULL) {
        status = -1;
        ESP_LOGE(KEY_CACHE_LOG_GROUP, "Failed to create Key reconstruction Cache!");
    }
    else
    {
        if (xSemaphoreTake(key_cache->xMutexCacheAccess, portMAX_DELAY))
        {   
            //Init cache
            for (int i = 0; i < key_cache->cache_size; i++)
            {
                key_cache->map[i].key_id = 0;
                reset_tick_count_for_map(&(key_cache->map[i]));
                memset(&(key_cache->map[i].key), 0, sizeof(key_cache->map[i].key));
            }
            xSemaphoreGive(key_cache->xMutexCacheAccess); // Release the mutex
        }
        else
        {
            ESP_LOGE(KEY_CACHE_LOG_GROUP, "Failed to acquire mutex for cache access");
            status = -2; // Mutex acquisition failure
        }
    }

    return status;
}

int add_key_to_cache(key_reconstruction_cache * const key_cache, key_128b * key, uint8_t key_id)
{
    if (key_cache == NULL)
    {
        return -3;
    }

    int status = 0;
    int first_free_index = -1;
    bool key_is_already_in_cache = false;

    if (xSemaphoreTake(key_cache->xMutexCacheAccess, portMAX_DELAY))
    {
        for (int i = 0; i < key_cache->cache_size; i++)
        {
            if (key_cache->map[i].key_id == 0 && first_free_index < 0)
            {
                first_free_index = i;
            }
            else if (key_cache->map[i].key_id == key_id)
            {
                key_is_already_in_cache = true;
            }
        }

        if (first_free_index < 0)
        {
            status = -1; 
        }
        else if (key_is_already_in_cache == true)
        {
            status = 0;
        }
        else
        {
            save_tick_count_for_key_map(&(key_cache->map[first_free_index]));
            key_cache->map[first_free_index].key_id = key_id;
            memcpy(&(key_cache->map[first_free_index].key), key, sizeof(key_cache->map[first_free_index].key));
            status = 0;
            if (key_cache->last_key_id_used == -1 && key_cache->last_key_index_in_map == -1)
            {
                key_cache->last_key_id_used = key_id;
                key_cache->last_key_index_in_map = first_free_index;
            }
        }

        xSemaphoreGive(key_cache->xMutexCacheAccess); // Release the mutex
    }
    else
    {
        ESP_LOGE(KEY_CACHE_LOG_GROUP, "Failed to acquire mutex for cache access");
        status = -2; // Mutex acquisition failure
    }

    return status;

}

int remove_key_from_cache(key_reconstruction_cache * const key_cache, uint8_t key_id)
{
    if (key_cache == NULL)
    {
        return -3;
    }

    int status = 0;
    if (xSemaphoreTake(key_cache->xMutexCacheAccess, portMAX_DELAY))
    {
        int key_index_in_map = -1;
        for (int i = 0; i < key_cache->cache_size; i++)
        {
            if (key_cache->map[i].key_id == key_id)
            {
                key_index_in_map = i;
                break;
            }
        }

        if (key_index_in_map >= 0)
        {
            reset_tick_count_for_map(&(key_cache->map[key_index_in_map]));
            key_cache->map[key_index_in_map].key_id = 0;
            memset(&(key_cache->map[key_index_in_map].key), 0, sizeof(key_cache->map[key_index_in_map].key));

            if (key_id == key_cache->last_key_id_used)
            {
                key_cache->last_key_id_used = -1;
                key_cache->last_key_index_in_map = -1;
            }

        }

        xSemaphoreGive(key_cache->xMutexCacheAccess); // Release the mutex
    }
    else
    {
        ESP_LOGE(KEY_CACHE_LOG_GROUP, "Failed to acquire mutex for cache access");
        status = -2; // Mutex acquisition failure
    }

    return status;
}

const key_128b* get_key_from_cache(key_reconstruction_cache * const key_cache, uint8_t key_id)
{

    key_128b* key = NULL;

    if (key_cache == NULL)
    {
        return key;
    }

    if (xSemaphoreTake(key_cache->xMutexCacheAccess, portMAX_DELAY))
    {
        if (key_id == key_cache->last_key_id_used && key_cache->last_key_index_in_map >= 0)
        {
            key = &(key_cache->map[key_cache->last_key_index_in_map].key);
            save_tick_count_for_key_map(&(key_cache->map[key_cache->last_key_index_in_map]));
            xSemaphoreGive(key_cache->xMutexCacheAccess); // Release the mutex
            return key;
        }


        int key_index_in_map = -1;
        for (int i = 0; i < key_cache->cache_size; i++)
        {
            if (key_cache->map[i].key_id == key_id)
            {
                key_index_in_map = i;
                break;
            }
        }

        if (key_index_in_map >= 0)
        {
            key = &(key_cache->map[key_index_in_map].key);
            key_cache->last_key_id_used = key_id;
            key_cache->last_key_index_in_map = key_index_in_map;
            save_tick_count_for_key_map(&(key_cache->map[key_index_in_map]));
        }

        xSemaphoreGive(key_cache->xMutexCacheAccess); // Release the mutex
    }
    else
    {
        ESP_LOGE(KEY_CACHE_LOG_GROUP, "Failed to acquire mutex for cache access");
    } 

    return key;
}

bool is_key_in_cache(key_reconstruction_cache * const key_cache, uint8_t key_id)
{

    bool status = false;

    if (key_cache == NULL)
    {
        return -3;
    }


    if (xSemaphoreTake(key_cache->xMutexCacheAccess, portMAX_DELAY))
    {

        if (key_id == key_cache->last_key_id_used && key_cache->last_key_index_in_map >=0)
        {
            status = true;
        }
        else
        {
            for (int i = 0; i < key_cache->cache_size; i++)
            {
                if (key_cache->map[i].key_id == key_id)
                {
                    status = true;
                    break;
                }
            }
        }
        xSemaphoreGive(key_cache->xMutexCacheAccess); // Release the mutex
    }
    else
    {
        ESP_LOGE(KEY_CACHE_LOG_GROUP, "Failed to acquire mutex for cache access");
    }

    return status;
}

int remove_key_from_cache_at_index(key_reconstruction_cache * const key_cache, uint8_t index)
{
    int status = 0;

    if (key_cache == NULL)
    {
        return -3;
    }

    if (xSemaphoreTake(key_cache->xMutexCacheAccess, portMAX_DELAY))
    {

        if (index > key_cache->cache_size)
        {
            status = -1;
        }
        else
        {
            uint8_t key_id = key_cache->map[index].key_id;
            reset_tick_count_for_map(&(key_cache->map[index]));
            key_cache->map[index].key_id = 0;
            memset(&(key_cache->map[index].key), 0, sizeof(key_cache->map[index].key));

            if (key_id == key_cache->last_key_id_used)
            {
                key_cache->last_key_id_used = -1;
                key_cache->last_key_index_in_map = -1;
            }
        }
        xSemaphoreGive(key_cache->xMutexCacheAccess); // Release the mutex
    }
    else
    {
        ESP_LOGE(KEY_CACHE_LOG_GROUP, "Failed to acquire mutex for cache access");
    }

    return status;
}

int remove_lru_key_from_cache(key_reconstruction_cache * const key_cache)
{
    if (key_cache == NULL)
    {
        return -1;
    }

    int lru_index = -1;
    uint64_t min_combined_timestamp = UINT64_MAX;


    if (xSemaphoreTake(key_cache->xMutexCacheAccess, portMAX_DELAY))
    {
        for (int i = 0; i < key_cache->cache_size; i++)
        {
            uint64_t combined_timestamp = get_combined_timestamp(&(key_cache->map[i]));
            if (combined_timestamp < min_combined_timestamp && key_cache->map[i].key_id != 0)
            {
                min_combined_timestamp = combined_timestamp;
                lru_index = i;
            }
        }
        xSemaphoreGive(key_cache->xMutexCacheAccess); // Release the mutex
    }

    if (lru_index >= 0)
    {
        return remove_key_from_cache_at_index(key_cache, lru_index);
    }

    return -1; // No keys found
}

bool clear_cache(key_reconstruction_cache * const key_cache)
{
    bool cache_clear_done = false;
    if (key_cache != NULL)
    {
        for (int i = 0; i < key_cache->cache_size; i++)
        {
            cache_clear_done = remove_key_from_cache_at_index(key_cache, i) == 0;
        }
    }

    return cache_clear_done;
}