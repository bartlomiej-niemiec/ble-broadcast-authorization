#include "key_cache.h"
#include "esp_log.h"
#include "string.h"
#include "freertos/task.h"


static const char *KEY_CACHE_LOG_GROUP = "KEY CACHE LOG";


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
                key_cache->map[i].last_used_timestamp = 0;
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
            key_cache->map[first_free_index].last_used_timestamp = xTaskGetTickCount();
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
            key_cache->map[key_index_in_map].last_used_timestamp = 0;
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
            key_cache->map[key_cache->last_key_index_in_map].last_used_timestamp = xTaskGetTickCount();
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
            key_cache->map[key_index_in_map].last_used_timestamp = xTaskGetTickCount();
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
            key_cache->map[index].last_used_timestamp = 0;
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
        return -3;
    }

    int min_index = 0;
    uint32_t min_timestamp = key_cache->map[0].last_used_timestamp;
    for (int i = 0; i < key_cache->cache_size; i++)
    {
        if (key_cache->map[i].last_used_timestamp < min_timestamp)
        {
            min_timestamp = key_cache->map[i].last_used_timestamp;
            min_index = i;
        }
    }

    return remove_key_from_cache_at_index(key_cache, min_index);
}