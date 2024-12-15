//My libs
#include "dispatcher.h"
#include "beacon_pdu_data.h"
#include "sec_pdu_processing.h"

//FreeRtos
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"

//Esp32
#include "esp_log.h"

//System
#include <stddef.h>
#include <string.h>
#include <limits.h> 

#define MAX_NO_ELEMENTS_IN_BUFFER 10
#define BUFFER_SIZE ((MAX_NO_ELEMENTS_IN_BUFFER) * (MAX_BLE_BROADCAST_SIZE_BYTES))
#define TASK_DELAY_MS 10

#define DISPATCHER_TASK_SIZE 1024
#define DISPATCHER_TASK_PRIORITY 6
#define DISPATCHER_TASK_CORE 1

static const char* DIST_LOG_GROUP = "DISPATCHER TASK";
static const char* DISTRIBUATION_TASK_NAME = "DISPATCHER TASK";

static TaskHandle_t xDispatcherTask = NULL;
static RingbufHandle_t ringBuffer;


void dispatcher_main(void *arg)
{
    while (1)
    {
        size_t itemSize = 0U;

        // Try to retriev data from the ring buffer with a timeout of 0
        uint8_t *data = (uint8_t *) xRingbufferReceive(ringBuffer, &itemSize, pdMS_TO_TICKS(TASK_DELAY_MS));

        if (data != NULL) {

            ble_broadcast_pdu* pdu_struct = (ble_broadcast_pdu *) data;

            if (true == is_pdu_in_beacon_pdu_format(pdu_struct->data, pdu_struct->data_len))
            {
                process_sec_pdu((beacon_pdu_data *) pdu_struct->data);
            }

            // Return the memory to the ring buffer
            vRingbufferReturnItem(ringBuffer, (void *) data);
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS)); 
    }
}

int init_dispatcher_resources()
{
    int status = 0;

    ringBuffer = xRingbufferCreate(BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (ringBuffer == NULL) {
        status = -1;
        ESP_LOGE(DIST_LOG_GROUP, "Ring buffer allocation failed!");
    }

    return status;

}

bool create_ble_broadcast_pdu_for_dispatcher(ble_broadcast_pdu* pdu, uint8_t *data, size_t size)
{
    bool result = false;
    if (size <= MAX_BLE_BROADCAST_SIZE_BYTES)
    {
        memcpy(pdu->data, data, size);
        memcpy(&(pdu->data_len), &size, sizeof(size));
        result = true;
    }
    else
    {
        ESP_LOGE(DIST_LOG_GROUP, "Size of data larger than pdu->data!");
    }
    return result;
}

int queue_pdu_for_dispatching(ble_broadcast_pdu* pdu)
{
    int status = 0;
    if (xDispatcherTask != NULL)
    {
        status = xRingbufferSend(ringBuffer, pdu, sizeof(ble_broadcast_pdu), pdMS_TO_TICKS(10));
        if (status != pdTRUE)
        {
            status = -2;
            ESP_LOGE(DIST_LOG_GROUP, "Adding element to RingBufferFailed!");
        }
    }
    else
    {
        status = -1;
    }
    return status;
}

int start_up_dispatcher()
{

    int status = 0;
    bool is_initialized = init_dispatcher_resources() == 0 ? true : false;

    if (is_initialized == false)
    {
        ESP_LOGE(DIST_LOG_GROUP, "Init failed! :(");
        status = -1;
    }
    else
    {
        if (xDispatcherTask == NULL)
        {
            BaseType_t  taskCreateResult = xTaskCreatePinnedToCore(
                dispatcher_main,
                DISTRIBUATION_TASK_NAME, 
                (uint32_t) DISPATCHER_TASK_SIZE,
                NULL,
                (UBaseType_t) DISPATCHER_TASK_PRIORITY,
                &xDispatcherTask,
                DISPATCHER_TASK_CORE
                );
        
            if (taskCreateResult != pdPASS)
            {
                status = -2;
                ESP_LOGE(DIST_LOG_GROUP, "Task was not created successfully! :(");
            }
            else
            {
                ESP_LOGI(DIST_LOG_GROUP, "Task was created successfully! :)");
            }
        
        }
    }

    return status;
}







