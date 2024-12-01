#include "beacon_pdu_data.h"
#include "dispatcher.h"
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"

static const char* DIST_LOG_GROUP = "DISTRIBUATION TASK";
static const char* DISTRIBUATION_TASK_NAME = "DISPATCHER TASK";
TaskHandle_t *xDistribuationTask = NULL;
RingbufHandle_t ringBuffer;

#define MAX_NO_ELEMENTS_IN_BUFFER 10

#define BUFFER_SIZE ((MAX_NO_ELEMENTS_IN_BUFFER) * (MAX_BLE_BROADCAST_SIZE_BYTES))
#define TASK_DELAY_MS 10

void DispatcherTaskMain();

void create_ble_broadcast_pdu_for_dispatcher(ble_broadcast_pdu* pdu, uint8_t *data, size_t size, uint64_t timeinterval)
{
    pdu->timeinterval = timeinterval;
    if (size <= MAX_BLE_BROADCAST_SIZE_BYTES)
    {
        memcpy(pdu->data, data, size);
    }
    else if (size > MAX_BLE_BROADCAST_SIZE_BYTES)
    {
        ESP_LOGE(DIST_LOG_GROUP, "Size of data larger than pdu->data!");
        memcpy(pdu->data, data, MAX_BLE_BROADCAST_SIZE_BYTES);
    }
}

void AddPduToDispatcher(ble_broadcast_pdu* pdu)
{
    int status = xRingbufferSend(ringBuffer, pdu, sizeof(ble_broadcast_pdu), pdMS_TO_TICKS(10));
    if (status != pdTRUE)
    {
        ESP_LOGE(DIST_LOG_GROUP, "Adding element to RingBufferFailed!");
    }
}

void SpawnDispatcherTask(void) {
    if (xDistribuationTask == NULL)
    {
        BaseType_t  taskCreateResult = xTaskCreatePinnedToCore(
            DispatcherTaskMain,
            DISTRIBUATION_TASK_NAME, 
            (uint32_t) DISPATCHER_TASK_SIZE,
            NULL,
            (UBaseType_t) DISPATCHER_TASK_PRIORITY,
            xDistribuationTask,
            DISPATCHER_TASK_CORE
            );
    
        if (taskCreateResult != pdPASS)
        {
            ESP_LOGE(DIST_LOG_GROUP, "Task was not created successfully! :(");
        }
        else
        {
            ESP_LOGI(DIST_LOG_GROUP, "Task was created successfully! :)");
        }
    
    }

}

void PassPduToHigherLayers(uint8_t * data, size_t pdu_size)
{
    if (pdu_size == sizeof(ble_broadcast_pdu))
    {
        ble_broadcast_pdu* pdu_struct = (ble_broadcast_pdu *) data;
        beacon_pdu_data* bpd = (beacon_pdu_data*) pdu_struct->data;
        ESP_LOGE(DIST_LOG_GROUP, "Get PDU with timestamp: %llu", pdu_struct->timeinterval);
        if (bpd->bcd.key_id == 0 && bpd->bcd.key_fragment_no == 0)
        {
            return;
        }
        else
        {
            
        }
    }
}

void DispatcherTaskMain(void *arg)
{
    ringBuffer = xRingbufferCreate(BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (ringBuffer == NULL) {
        ESP_LOGE(DIST_LOG_GROUP, "Ring buffer allocation failed!");
    }

    while (1)
    {
        size_t itemSize = 0U;

        // Try to retriev data from the ring buffer with a timeout of 0
        uint8_t *packet = (uint8_t *) xRingbufferReceive(ringBuffer, &itemSize, pdMS_TO_TICKS(0));

        if (packet != NULL) {


            PassPduToHigherLayers(packet, itemSize);

            // Return the memory to the ring buffer
            vRingbufferReturnItem(ringBuffer, (void *) packet);
        }
        else
        {
            // No data in ring buffer, delay to avoid busy waiting
            vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS)); 
        }
    }
}







