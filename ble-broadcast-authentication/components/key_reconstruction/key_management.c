#include <stdint.h>
#include <string.h>
#include "key_management.h"
#include "esp_log.h"

static const char* KEY_MNGMT_GROUP = "KEY_MANAGEMENT_GROUP";

key_management key_collections[KEY_COLLECTIONS_SIZE];

void init_key_management(key_management* km)
{
    memset((void *) &(km->key), 0, sizeof(key_128b));
    memset((void *) &(km->key_fragments), 0, sizeof(key_splitted));
    km->key_id = 0;
    km->no_collected_key_fragments = 0;
}

void reconstruct_key_from_key_fragments(key_management* km)
{
    get_128b_key_from_fragments(&km->key, &km->key_fragments);
}

bool is_key_in_collection(uint8_t key_id)
{
    bool in_collection = false;
    for (int i = 0; i < KEY_COLLECTIONS_SIZE; i++)
    {
        if (key_collections[i].key_id == key_id)
        {
            in_collection = true;
            break;
        }
    }
    return in_collection;
}

bool add_new_key_to_collection(uint8_t key_id)
{
    bool result = false;

    for (int i = 0; i < KEY_COLLECTIONS_SIZE; i++)
    {
        if (key_collections[i].key_id == 0)
        {
            key_collections[i].key_id = key_id;
            result = true;
            break;
        }
    }

    return result;
}

void add_fragment_to_key_management(uint8_t key_id, uint8_t *fragment, uint8_t key_fragment_id)
{
    int key_index_in_collection;
    bool key_found = false; 
    for (key_index_in_collection = 0; key_index_in_collection < KEY_COLLECTIONS_SIZE; key_index_in_collection++)
    {
        if (key_collections[key_index_in_collection].key_id == key_id)
        {
            key_found = true;
            break;
        }
    }

    if (key_found == true)
    {
        add_fragment_to_key_spliited(&(key_collections[key_index_in_collection].key_fragments), fragment, key_fragment_id);
        key_collections[key_index_in_collection].no_collected_key_fragments++;
    }
    else
    {
        ESP_LOGE(KEY_MNGMT_GROUP, "Key has not been found!");
    }
}

bool is_key_available(uint8_t key_id)
{
    bool key_is_available = false;
    for (int i = 0; i < KEY_COLLECTIONS_SIZE; i++)
    {
        if (key_collections[i].key_id == key_id)
        {
            key_is_available = key_collections[i].no_collected_key_fragments == 3 ? true : false;
            break;
        }
    }

    return key_is_available;
}
