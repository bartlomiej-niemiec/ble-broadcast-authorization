#include "beacon_pdu_data.h"
#include "key_reconstructor.h"
#include "dispatcher.h"
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"
#include <limits.h> 

#define MAX_NO_ELEMENTS_IN_BUFFER 10
#define BUFFER_SIZE ((MAX_NO_ELEMENTS_IN_BUFFER) * (MAX_BLE_BROADCAST_SIZE_BYTES))
#define TASK_DELAY_MS 10

#define DISPATCHER_TASK_SIZE 4096
#define DISPATCHER_TASK_PRIORITY 8
#define DISPATCHER_TASK_CORE 1

static const char* DIST_LOG_GROUP = "DISPATCHER TASK";
static const char* DISTRIBUATION_TASK_NAME = "DISPATCHER TASK";
TaskHandle_t *xDistribuationTask = NULL;
RingbufHandle_t ringBuffer;

void key_reconstruction_complete(uint8_t key_id, const key_128b * const reconstructed_key)
{
    ESP_LOGI(DISTRIBUATION_TASK_NAME, "Key reconstruction success!");
    ESP_LOGI(DISTRIBUATION_TASK_NAME, "Reconstructed key id: %i", key_id);
    ESP_LOG_BUFFER_HEX("KEY RECONSTRUCTED HEX", reconstructed_key, sizeof(key_128b));
}

void DispatcherTaskMain();

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

void queue_pdu_for_dispatching(ble_broadcast_pdu* pdu)
{
    int status = xRingbufferSend(ringBuffer, pdu, sizeof(ble_broadcast_pdu), pdMS_TO_TICKS(10));
    if (status != pdTRUE)
    {
        ESP_LOGE(DIST_LOG_GROUP, "Adding element to RingBufferFailed!");
    }
}

void start_up_dispatcher(void) {
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
            register_callback_to_key_reconstruction(key_reconstruction_complete);
        }
    
    }

}

void PassPduToHigherLayers(ble_broadcast_pdu * pdu)
{
    beacon_pdu_data *pdu_beacon = (beacon_pdu_data *) pdu->data;
    queue_key_for_reconstruction(pdu_beacon->bcd.key_id, pdu_beacon->bcd.key_fragment_no, pdu_beacon->bcd.enc_key_fragment, pdu_beacon->bcd.key_fragment_hmac, pdu_beacon->bcd.xor_seed);
}

void LogPduData(beacon_pdu_data * pdu)
{
    ESP_LOGI(DIST_LOG_GROUP, "CRYPTO DATA: Key ID: %i", pdu->bcd.key_id);
    ESP_LOGI(DIST_LOG_GROUP, "CRYPTO DATA: Key FRAGMENT NO: %i", pdu->bcd.key_fragment_no);
    ESP_LOGI(DIST_LOG_GROUP, "CRYPTO DATA: XOR SEED: %i", pdu->bcd.xor_seed);

    ESP_LOG_BUFFER_HEX("CRYPTO DATA: ENCRYPTED KEY FRAGMENT", pdu->bcd.enc_key_fragment, sizeof(pdu->bcd.enc_key_fragment));
    ESP_LOG_BUFFER_HEX("CRYPTO DATA: KEY FARGMENT HMAC", pdu->bcd.key_fragment_hmac, sizeof(pdu->bcd.key_fragment_hmac));
    ESP_LOG_BUFFER_HEX("CRYPTO DATA: PAYLOAD", pdu->payload, sizeof(pdu->payload));
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
        uint8_t *data = (uint8_t *) xRingbufferReceive(ringBuffer, &itemSize, pdMS_TO_TICKS(TASK_DELAY_MS));

        if (data != NULL) {

            ble_broadcast_pdu* pdu_struct = (ble_broadcast_pdu *) data;

            if (true == is_pdu_in_beacon_pdu_format(pdu_struct->data, pdu_struct->data_len))
            {
                // beacon_pdu_data *pdu_beacon = (beacon_pdu_data *) pdu_struct->data;
                // LogPduData(pdu_beacon);
                PassPduToHigherLayers(pdu_struct);
            }

            // Return the memory to the ring buffer
            vRingbufferReturnItem(ringBuffer, (void *) data);
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS)); 
    }
}







