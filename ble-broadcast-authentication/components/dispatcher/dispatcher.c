#include "beacon_pdu_data.h"
#include "dispatcher.h"
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"
#include <limits.h> 

static const char* DIST_LOG_GROUP = "DISTRIBUATION TASK";
static const char* DISTRIBUATION_TASK_NAME = "DISPATCHER TASK";
TaskHandle_t *xDistribuationTask = NULL;
RingbufHandle_t ringBuffer;
int64_t prev_timestamp = 0;

// Calculate the interval with wrap-around handling for int64_t timestamps
int64_t calculate_timestamp_diff(int64_t current_timestamp, int64_t previous_timestamp) {
    // Handle normal difference (current_timestamp >= previous_timestamp)
    if (current_timestamp >= previous_timestamp) {
        return current_timestamp - previous_timestamp;
    } else {
        // Handle wrap-around in 64-bit time (since ESP timer is 64-bit)
        // ESP timer will wrap around after INT64_MAX microseconds
        int64_t max_time = INT64_MAX;
        int64_t time_diff = (max_time - previous_timestamp) + current_timestamp + 1;

        // Ensure the calculated time difference is valid and not negative
        if (time_diff < 0) {
            // This case can occur in extreme wrap-around conditions, fallback to a small value
            return 0;
        }

        return time_diff;
    }
}

// Round to the nearest multiple of n, rounding up if more than half of n
uint32_t round_up_to_nearest(uint32_t value, uint32_t n) {
    uint32_t remainder = value % n;
    
    if (remainder == 0) {
        return value;  // Already a multiple of n
    }

    if (remainder >= (n / 2)) {
        // Round up if remainder is greater than or equal to half of n
        return ((value / n) + 1) * n;
    } else {
        // Round down if remainder is less than half of n
        return (value / n) * n;
    }
}

#define MAX_NO_ELEMENTS_IN_BUFFER 10

#define BUFFER_SIZE ((MAX_NO_ELEMENTS_IN_BUFFER) * (MAX_BLE_BROADCAST_SIZE_BYTES))
#define TASK_DELAY_MS 10

void DispatcherTaskMain();

void create_ble_broadcast_pdu_for_dispatcher(ble_broadcast_pdu* pdu, uint8_t *data, size_t size, int64_t timeinterval)
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
        uint16_t time_interval_sender;
        memcpy(&time_interval_sender, bpd->payload, sizeof(uint16_t));
        ESP_LOGE(DIST_LOG_GROUP, "Sender time interval: %u", time_interval_sender);
        
        // Calculate timestamp difference in microseconds
        int64_t timestamp_diff = calculate_timestamp_diff(pdu_struct->timeinterval, prev_timestamp);
        
        // Convert timestamp_diff to milliseconds and round up to the nearest 100ms
        uint32_t time_interval_ms = (uint32_t)(timestamp_diff / 1000); // Convert to ms
        ESP_LOGE(DIST_LOG_GROUP, "Receiver calculated interval (not-rounded): %lu ms", time_interval_ms);
        uint32_t time_interval_ms_round_up = round_up_to_nearest(time_interval_ms, 100); // Round up to nearest 100ms
        ESP_LOGE(DIST_LOG_GROUP, "Receiver calculated interval (rounded): %lu ms", time_interval_ms_round_up);

        prev_timestamp = pdu_struct->timeinterval;

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







