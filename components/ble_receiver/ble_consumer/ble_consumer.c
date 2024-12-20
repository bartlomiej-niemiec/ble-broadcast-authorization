
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
        p_ble_consumer->context.process_deferred_q_request_pending = false;
        create_key_cache(&(p_ble_consumer->context.key_cache), key_cache_size);
        p_ble_consumer->context.deferredQueue = xQueueCreate(DEFERRED_QUEUE_SIZE, sizeof(beacon_pdu_data));
        if (p_ble_consumer->context.deferredQueue == NULL)
        {
            free(p_ble_consumer->context.key_cache);
        }
        else
        {
            status = -1;
        }

        if (status == 0)
        {
            p_ble_consumer->xMutex = xSemaphoreCreateMutex();
            if (p_ble_consumer->xMutex == NULL)
            {
                status = -2;
            }
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
        p_ble_consumer->context.process_deferred_q_request_pending = false;
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
        if (xSemaphoreTake(p_ble_consumer->xMutex, portMAX_DELAY) == pdTRUE)
        {
            p_ble_consumer->context.process_deferred_q_request_pending = false;
            p_ble_consumer->last_pdu_timestamp = 0;
            p_ble_consumer->context.recently_removed_key_id =-1;
            p_ble_consumer->context.deferred_queue_count = 0;
            p_ble_consumer->rollover = 0;
            memset(&(p_ble_consumer->mac_address_arr), 0, sizeof(esp_bd_addr_t));
            clear_cache(p_ble_consumer->context.key_cache);
            xQueueReset(p_ble_consumer->context.deferredQueue);
            xSemaphoreGive(p_ble_consumer->xMutex);
        }
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
        if (xSemaphoreTake(p_ble_consumer->xMutex, portMAX_DELAY) == pdTRUE)
        {
            if (p_ble_consumer->context.deferredQueue != NULL)
            {
                vQueueDelete(p_ble_consumer->context.deferredQueue);
            }
            if (p_ble_consumer->context.key_cache != NULL)
            {
                destroy_key_cache(p_ble_consumer->context.key_cache);
            }
            xSemaphoreGive(p_ble_consumer->xMutex);
        }
        vSemaphoreDelete(p_ble_consumer->xMutex);
    }
    return 0;
}

bool is_pdu_in_deferred_queue(ble_consumer *p_ble_consumer)
{
    bool pdu_in_q = false;
    if (p_ble_consumer != NULL)
    {
        if (xSemaphoreTake(p_ble_consumer->xMutex, portMAX_DELAY) == pdTRUE)
        {
            pdu_in_q = p_ble_consumer->context.deferred_queue_count > 0 ? true: false;
            xSemaphoreGive(p_ble_consumer->xMutex);
        }
    }
    return pdu_in_q;
}


int add_to_deferred_queue(ble_consumer* p_ble_consumer, beacon_pdu_data* pdu)
{
    int status = 0;
    if (p_ble_consumer != NULL)
    {
        if (xSemaphoreTake(p_ble_consumer->xMutex, portMAX_DELAY) == pdTRUE)
        {   
            status = xQueueSend(p_ble_consumer->context.deferredQueue, (void *) pdu, portMAX_DELAY);
            p_ble_consumer->context.deferred_queue_count++;
            xSemaphoreGive(p_ble_consumer->xMutex);
        }
    }
    return status;
}

bool get_deferred_queue_item(ble_consumer* p_ble_consumer, beacon_pdu_data* pdu)
{
    BaseType_t status = pdFALSE;
    beacon_pdu_data temp_pdu;
    if (p_ble_consumer != NULL && pdu != NULL)
    {
        if (xSemaphoreTake(p_ble_consumer->xMutex, portMAX_DELAY) == pdTRUE)
        {   
            status = xQueueReceive(p_ble_consumer->context.deferredQueue, (void *) &pdu, portMAX_DELAY);
            p_ble_consumer->context.deferred_queue_count--;
            xSemaphoreGive(p_ble_consumer->xMutex);
        }
    }
    return status;
}

void set_deferred_q_pending_processing(ble_consumer* p_ble_consumer, const bool request)
{
    if (p_ble_consumer != NULL)
    {
        if (xSemaphoreTake(p_ble_consumer->xMutex, portMAX_DELAY) == pdTRUE)
        {   
            p_ble_consumer->context.process_deferred_q_request_pending = request;
            xSemaphoreGive(p_ble_consumer->xMutex);
        }
    }
}

bool is_deferred_queue_request_pending(ble_consumer* p_ble_consumer)
{
    bool is_pending = false;
    if (p_ble_consumer != NULL)
    {
        if (xSemaphoreTake(p_ble_consumer->xMutex, portMAX_DELAY) == pdTRUE)
        {   
            is_pending = p_ble_consumer->context.process_deferred_q_request_pending;
            xSemaphoreGive(p_ble_consumer->xMutex);
        }
    }
    return is_pending;
}