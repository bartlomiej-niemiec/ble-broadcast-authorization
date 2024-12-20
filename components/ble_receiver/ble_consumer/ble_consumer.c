
#include "ble_consumer.h"
#include "freertos/FreeRTOS.h"
#include <string.h>
#include "beacon_pdu_data.h"

ble_consumer * create_ble_consumer(const uint8_t key_cache_size)
{
    ble_consumer * p_ble_consumer = NULL;
    p_ble_consumer = (ble_consumer *) malloc(sizeof(ble_consumer));
    if (p_ble_consumer != NULL)
    {
        p_ble_consumer->context.key_cache_size = key_cache_size;
        create_key_cache(&(p_ble_consumer->context.key_cache), key_cache_size);
        p_ble_consumer->context.deferredQueue = xQueueCreate(DEFERRED_QUEUE_SIZE, sizeof(beacon_pdu_data));
        if (p_ble_consumer->context.deferredQueue == NULL)
        {
            free(p_ble_consumer->context.key_cache);
            free(p_ble_consumer);
            p_ble_consumer = NULL;
        }

    }

    return p_ble_consumer;
}

int create_ble_consumer_resources(ble_consumer *const p_ble_consumer, const uint8_t key_cache_size)
{
    int status = -1;
    if (p_ble_consumer != NULL)
    {
        p_ble_consumer->context.key_cache_size = key_cache_size;
        create_key_cache(&(p_ble_consumer->context.key_cache), key_cache_size);
        p_ble_consumer->context.deferredQueue = xQueueCreate(DEFERRED_QUEUE_SIZE, sizeof(beacon_pdu_data));
        if (p_ble_consumer->context.deferredQueue == NULL)
        {
            free(p_ble_consumer->context.key_cache);
        }
        else
        {
            status = 0;
        }

    }
    return status;
}

int init_ble_consumer(ble_consumer *p_ble_consumer)
{
    int status = 0;
    if (p_ble_consumer == NULL)
    {
        status = -1;
    }
    else
    {
        p_ble_consumer->last_pdu_timestamp = 0; 
        p_ble_consumer->context.recently_removed_key_id = 0;
        p_ble_consumer->context.deferred_queue_count = 0;
        p_ble_consumer->rollover = 0;
        memset(p_ble_consumer->mac_address_arr, 0, sizeof(p_ble_consumer->mac_address_arr));
        init_key_cache(p_ble_consumer->context.key_cache);
    }

    return status;
}

int reset_ble_consumer(ble_consumer * p_ble_consumer)
{
    int status = 0;

    if (p_ble_consumer == NULL)
    {
        status = -1;
    }
    else
    {
        p_ble_consumer->last_pdu_timestamp = 0;
        p_ble_consumer->context.recently_removed_key_id =-1;
        p_ble_consumer->context.deferred_queue_count = 0;
        p_ble_consumer->rollover = 0;
        memset(&(p_ble_consumer->mac_address_arr), 0, sizeof(esp_bd_addr_t));
        clear_cache(p_ble_consumer->context.key_cache);
        xQueueReset(p_ble_consumer->context.deferredQueue);
    }
    return status;
}

int destroy_ble_consumer(ble_consumer *p_ble_consumer)
{
    if (NULL == p_ble_consumer)
    {
        return -1;
    }
    else
    {
        if (p_ble_consumer->context.deferredQueue != NULL)
        {
            vQueueDelete(p_ble_consumer->context.deferredQueue);
        }
        if (p_ble_consumer->context.key_cache != NULL)
        {
            destroy_key_cache(p_ble_consumer->context.key_cache);
        }
    }
    return 0;
}