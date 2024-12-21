#include <stdint.h>
#include <string.h>
#include "key_management.h"
#include "esp_log.h"
#include "esp_err.h"

static const char* KEY_MNGMT_GROUP = "KEY_MANAGEMENT_GROUP";

int get_key_index_in_collection(key_reconstruction_collection* key_collection, const esp_bd_addr_t consumer_mac_address, uint8_t key_id);

key_reconstruction_collection* create_new_key_collection(const size_t key_collection_size)
{
    key_reconstruction_collection* p_key_collection = NULL;
    p_key_collection = (key_reconstruction_collection*) malloc(sizeof(key_reconstruction_collection));
    if (p_key_collection != NULL)
    {
        memset(p_key_collection, 0, sizeof(key_reconstruction_collection)); // Zero-initialize all members

        p_key_collection->key_management_size = key_collection_size;
        p_key_collection->km = (key_management*) calloc(key_collection_size, sizeof(key_management));
        if (p_key_collection->km == NULL) {
            ESP_LOGE(KEY_MNGMT_GROUP, "Memory allocation for key management failed!");
            free(p_key_collection);
            return NULL;
        }

        p_key_collection->xMutex = xSemaphoreCreateRecursiveMutex();
        if (p_key_collection->xMutex == NULL) {
            ESP_LOGE(KEY_MNGMT_GROUP, "Failed to create mutex for key collection!");
            free(p_key_collection->km);
            free(p_key_collection);
            return NULL;
        }
    }
    
    return p_key_collection;
}

void remove_key_from_collection(key_reconstruction_collection* key_collection, const esp_bd_addr_t consumer_mac_address, uint8_t key_id)
{
    if (xSemaphoreTake(key_collection->xMutex, portMAX_DELAY))
    {
        int key_index_in_collection = get_key_index_in_collection(key_collection, consumer_mac_address, key_id);
        if (key_index_in_collection >= 0)
        {
            memset(&(key_collection->km[key_index_in_collection]), 0, sizeof(key_management));
        }
        xSemaphoreGive(key_collection->xMutex);
    }
    else
    {
        ESP_LOGE(KEY_MNGMT_GROUP, "Failed to acquire mutex for adding a new key.");
    }
}

bool reconstruct_key_from_key_fragments(key_reconstruction_collection* key_collection, key_128b* km, const esp_bd_addr_t consumer_mac_address, uint8_t key_id)
{
    bool key_reconstruction_result = false;

    if (key_collection == NULL || km == NULL || consumer_mac_address == NULL)
    {
        ESP_LOGE(KEY_MNGMT_GROUP, "NULL PTR reconstruct_key_from_key_fragments");
        return key_reconstruction_result;
    }

    if (is_key_available(key_collection, consumer_mac_address, key_id) == true)
    {   
        if (xSemaphoreTake(key_collection->xMutex, portMAX_DELAY))
        {
            int key_index_in_collection = get_key_index_in_collection(key_collection, consumer_mac_address, key_id);
            get_128b_key_from_fragments(&(key_collection->km[key_index_in_collection].key), &(key_collection->km[key_index_in_collection].key_fragments));
            memcpy(km, &(key_collection->km[key_index_in_collection].key), sizeof(key_128b));
            key_reconstruction_result = true;
            xSemaphoreGive(key_collection->xMutex);
        }
        else
        {
            ESP_LOGE(KEY_MNGMT_GROUP, "Failed to acquire mutex for adding a new key.");
        }
    }
    return key_reconstruction_result;
}

bool is_key_in_collection(key_reconstruction_collection* key_collection, const esp_bd_addr_t consumer_mac_address, uint8_t key_id)
{
    bool result = false;
    if (xSemaphoreTake(key_collection->xMutex, portMAX_DELAY))
    {
        result = get_key_index_in_collection(key_collection, consumer_mac_address, key_id) >= 0 ? true : false;
        xSemaphoreGive(key_collection->xMutex);
    }
    else
    {
        ESP_LOGE(KEY_MNGMT_GROUP, "Failed to acquire mutex for adding a new key.");
    }
    return result;
}

bool add_new_key_to_collection(key_reconstruction_collection* key_collection, esp_bd_addr_t consumer_mac_address, uint8_t key_id)
{
    bool result = false;

    if (key_collection == NULL || consumer_mac_address == NULL)
    {
        return result;
    }

    if (xSemaphoreTake(key_collection->xMutex, portMAX_DELAY))
    {
        for (int i = 0; i < key_collection->key_management_size; i++)
        {
            if (key_collection->km[i].key_id == 0)
            {
                key_collection->km[i].key_id = key_id;
                memcpy(key_collection->km[i].consumer_mac_address, consumer_mac_address, sizeof(esp_bd_addr_t));
                result = true;
                break;
            }
        }
        xSemaphoreGive(key_collection->xMutex);
    }
    else
    {
        ESP_LOGE(KEY_MNGMT_GROUP, "Failed to acquire mutex for adding a new key.");
    }

    return result;
}

void add_fragment_to_key_management(key_reconstruction_collection* key_collection, const esp_bd_addr_t consumer_mac_address, uint8_t key_id, uint8_t *fragment, uint8_t key_fragment_id)
{
    if (xSemaphoreTake(key_collection->xMutex, portMAX_DELAY))
    {
        int key_index = get_key_index_in_collection(key_collection, consumer_mac_address, key_id);
        if (key_index >= 0)
        {
            add_fragment_to_key_spliited(&(key_collection->km[key_index].key_fragments), fragment, key_fragment_id);
            key_collection->km[key_index].decrypted_key_fragments[key_fragment_id] = true;
            key_collection->km[key_index].no_collected_key_fragments++;
        }
        xSemaphoreGive(key_collection->xMutex);
    }
    else
    {
        ESP_LOGE(KEY_MNGMT_GROUP, "Failed to acquire mutex for adding a new key.");
    }
}

bool is_key_available(key_reconstruction_collection* key_collection, const esp_bd_addr_t consumer_mac_address, uint8_t key_id)
{
    bool result = false;
    if (xSemaphoreTake(key_collection->xMutex, portMAX_DELAY))
    {
        int index = get_key_index_in_collection(key_collection, consumer_mac_address, key_id);
        if (index >= 0 )
        {
            result = key_collection->km[index].no_collected_key_fragments == 4 ? true : false;
        }
        xSemaphoreGive(key_collection->xMutex);
    }
    else
    {
        ESP_LOGE(KEY_MNGMT_GROUP, "Failed to acquire mutex for adding a new key.");
    }

    return result;
}

int get_key_index_in_collection(key_reconstruction_collection* key_collection, const esp_bd_addr_t consumer_mac_address, uint8_t key_id)
{
    int index = -1;
    if (key_collection == NULL || consumer_mac_address == NULL)
    {
        return index;
    }

    for (int i = 0; i < key_collection->key_management_size; i++)
    {
        if (key_collection->km[i].key_id == key_id && memcmp(key_collection->km[i].consumer_mac_address, consumer_mac_address, sizeof(esp_bd_addr_t)) == 0)
        {
            index = i;
            break;
        }
    }

    return index;
}

bool is_key_fragment_decrypted(key_reconstruction_collection* key_collection, const esp_bd_addr_t consumer_mac_address, uint8_t key_id, uint8_t key_fragment)
{
    bool key_fragment_decrypted = false;
    if (xSemaphoreTake(key_collection->xMutex, portMAX_DELAY))
    {
        int key_index_in_collection = get_key_index_in_collection(key_collection, consumer_mac_address, key_id);
        if (key_index_in_collection >= 0)
        {
            key_fragment_decrypted = key_collection->km[key_index_in_collection].decrypted_key_fragments[key_fragment] == true? true : false;
        }
        xSemaphoreGive(key_collection->xMutex);
    }
    else
    {
        ESP_LOGE(KEY_MNGMT_GROUP, "Failed to acquire mutex for adding a new key.");
    }

    return key_fragment_decrypted;
}
