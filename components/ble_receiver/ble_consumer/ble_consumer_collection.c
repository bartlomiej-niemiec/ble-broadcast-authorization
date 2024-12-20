#include "ble_consumer_collection.h"
#include <string.h>


void save_tick_count_for_ble_consumer(ble_consumer * const  p_ble_consumer)
{
    if (p_ble_consumer != NULL)
    {
        uint32_t tickCount = xTaskGetTickCount();
        if (tickCount < p_ble_consumer->last_pdu_timestamp)
        {
            p_ble_consumer->rollover++;
        }
        p_ble_consumer->last_pdu_timestamp = tickCount;
    }
}

uint64_t get_combined_timestamp(ble_consumer * const  p_ble_consumer)
{
    return ((uint64_t)p_ble_consumer->last_pdu_timestamp << 32) | p_ble_consumer->rollover;
}

void reset_tick_count_for_map(ble_consumer * const  p_ble_consumer)
{
    if (p_ble_consumer != NULL)
    {
        p_ble_consumer->rollover = 0;
        p_ble_consumer->last_pdu_timestamp = 0;
    }
}

ble_consumer_collection * create_ble_consumer_collection(const uint8_t collection_size, const uint8_t sender_key_cache_size)
{
    ble_consumer_collection * p_ble_consumer_collection = NULL;
    p_ble_consumer_collection = (ble_consumer_collection *) malloc(sizeof(ble_consumer_collection));
    if (p_ble_consumer_collection != NULL)
    {
        p_ble_consumer_collection->consumers_count = 0;
        p_ble_consumer_collection->size = collection_size;
        p_ble_consumer_collection->xMutex = NULL;
        p_ble_consumer_collection->xMutex = xSemaphoreCreateMutex();

        int malloc_counter = 0;
        bool malloc_failed = false;

        if (p_ble_consumer_collection->xMutex == NULL)
        {
            malloc_failed = true;
        }
        else
        {
            p_ble_consumer_collection->arr = (ble_consumer *) malloc(sizeof(ble_consumer) * collection_size);

            for (malloc_counter = 0; malloc_counter < collection_size; malloc_counter++)
            {
                int status = create_ble_consumer_resources(&p_ble_consumer_collection->arr[malloc_counter], sender_key_cache_size);
                if (status == -1)
                {
                    malloc_failed = true;
                    break;
                }
                init_ble_consumer(&p_ble_consumer_collection->arr[malloc_counter]);
            }
        }

        if (true == malloc_failed)
        {
            for (int i = 0; i < malloc_counter; i++)
            {
                destroy_ble_consumer(&(p_ble_consumer_collection->arr[i]));
            }
            free(p_ble_consumer_collection);
            p_ble_consumer_collection = NULL;
        }

    }
    return p_ble_consumer_collection;
}

int get_first_free_index(ble_consumer_collection * p_ble_consumer_collection)
{
    int index = -1;
    esp_bd_addr_t zer_mac_address = {0};
    if (p_ble_consumer_collection != NULL)
    {
        if (p_ble_consumer_collection->consumers_count < p_ble_consumer_collection->size)
        {
            for (int i = 0; i < p_ble_consumer_collection->size; i++)
            {
                if (0 == memcmp(&(p_ble_consumer_collection->arr[i].mac_address_arr), zer_mac_address, sizeof(esp_bd_addr_t)))
                {
                    index = i;
                    break;
                }
            }
        }
    }
    return index;
}

int get_index_for_mac_addr(ble_consumer_collection * p_ble_consumer_collection, esp_bd_addr_t mac_address_arr)
{
    int index = -1;
    if (p_ble_consumer_collection != NULL && mac_address_arr != NULL)
    {
        for (int i = 0; i < p_ble_consumer_collection->size; i++)
        {
            if (0 == memcmp(&(p_ble_consumer_collection->arr[i].mac_address_arr), mac_address_arr, sizeof(esp_bd_addr_t)))
            {
                index = i;
                break;
            }
        }
    }

    return index;
}


int clear_ble_consumer_from_collection(ble_consumer_collection * p_ble_consumer_collection, const uint8_t index)
{
    int status = -1;
    if (p_ble_consumer_collection != NULL)
    {
        reset_ble_consumer(&(p_ble_consumer_collection->arr[index]));
        p_ble_consumer_collection->consumers_count--;
    }
    return status;
}

int add_consumer_to_collection(ble_consumer_collection * p_ble_consumer_collection, esp_bd_addr_t mac_address_arr)
{
    int status = 0;
    if (p_ble_consumer_collection == NULL || mac_address_arr == NULL)
    {
        status = -1;
    }
    else
    {
        if (xSemaphoreTake(p_ble_consumer_collection->xMutex, portMAX_DELAY) == pdTRUE)
        {
            int index = get_first_free_index(p_ble_consumer_collection);
            if (index >= 0)
            {
                reset_ble_consumer(&(p_ble_consumer_collection->arr[index]));
                memcpy(&(p_ble_consumer_collection->arr[index].mac_address_arr), mac_address_arr, sizeof(esp_bd_addr_t));
                save_tick_count_for_ble_consumer(&(p_ble_consumer_collection->arr[index]));
                p_ble_consumer_collection->consumers_count++;
            }
        
            xSemaphoreGive(p_ble_consumer_collection->xMutex);
        }
    }
    return status;
}

ble_consumer * get_ble_consumer_from_collection(ble_consumer_collection * p_ble_consumer_collection, esp_bd_addr_t mac_address_arr)
{
    ble_consumer * p_ble_consumer = NULL;

    if (p_ble_consumer_collection != NULL && mac_address_arr != NULL)
    {
        if (xSemaphoreTake(p_ble_consumer_collection->xMutex, portMAX_DELAY) == pdTRUE)
        {
            int index = get_index_for_mac_addr(p_ble_consumer_collection, mac_address_arr);
            if (index >= 0)
            {
                p_ble_consumer = &(p_ble_consumer_collection->arr[index]);
                save_tick_count_for_ble_consumer(&(p_ble_consumer_collection->arr[index]));
            }   
            xSemaphoreGive(p_ble_consumer_collection->xMutex);
        }
    }

    return p_ble_consumer;

}

int remove_consumer_from_collection(ble_consumer_collection * p_ble_consumer_collection, esp_bd_addr_t mac_address_arr)
{
    int status = -1;
    if (p_ble_consumer_collection != NULL && mac_address_arr != NULL)
    {
        if (xSemaphoreTake(p_ble_consumer_collection->xMutex, portMAX_DELAY) == pdTRUE)
        {
            int index = get_index_for_mac_addr(p_ble_consumer_collection, mac_address_arr);
            if (index >= 0)
            {
                clear_ble_consumer_from_collection(p_ble_consumer_collection, index);
            }
            xSemaphoreGive(p_ble_consumer_collection->xMutex);
        }
        status = 0;
    }
    return status;
}

int remove_lru_consumer_from_collection(ble_consumer_collection * p_ble_consumer_collection)
{
    return -1;
}


int get_active_no_consumers(ble_consumer_collection * p_ble_consumer_collection)
{
    int active_consumers_count = -1;
    if (p_ble_consumer_collection == NULL)
    {
        return active_consumers_count;
    }

    if (xSemaphoreTake(p_ble_consumer_collection->xMutex, portMAX_DELAY) == pdTRUE)
    {
        active_consumers_count = p_ble_consumer_collection->consumers_count;
        xSemaphoreGive(p_ble_consumer_collection->xMutex);
    }

    return active_consumers_count;
}
