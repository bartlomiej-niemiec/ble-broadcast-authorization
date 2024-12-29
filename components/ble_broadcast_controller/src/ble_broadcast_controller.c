#include "ble_broadcast_controller.h"
#include "beacon_pdu_data.h"

#include "esp_log.h"
#include "esp_gap_ble_api.h"
#include "esp_timer.h"
#include "esp_random.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>

#define MAX_ELEMENTS_IN_QUEUE 10
#define MINUS_INTERVAL_TOLERANCE_MS 10
#define PLUS_INTERVAL_TOLERANCE_MS 10

#define BROADCAST_TASK_SIZE 4096
#define BROADCAST_TASK_PRIORITY 4
#define BROADCAST_TASK_CORE 1

#define SEMAPHORE_TIMEOUT_MS 20
#define QUEUE_TIMEOUT_MS 20

#define N_CONST 0.625
#define MS_TO_N_CONVERTION(MS) ((uint16_t)((MS) / (N_CONST)))

#define ADV_INT_MIN_MS 100
#define ADV_INT_MAX_MS 110

static const char* BROADCAST_TASK_NAME = "BROADCAST TASK";
static const char* BROADCAST_LOG_GROUP = "BROADCAST TASK";
static esp_ble_adv_params_t default_ble_adv_params = {
    .adv_int_min        = MS_TO_N_CONVERTION(ADV_INT_MIN_MS),
    .adv_int_max        = MS_TO_N_CONVERTION(ADV_INT_MAX_MS),
    .adv_type           = ADV_TYPE_NONCONN_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };

#define EVENT_QUEUE_SIZE 10

typedef struct {
    esp_gap_ble_cb_event_t event_type;
    uint8_t payload[MAX_PDU_PAYLOAD_SIZE];
    size_t payload_size;
} BroadcastEvent;

static QueueHandle_t eventQueue = NULL;


typedef struct {
    QueueHandle_t eventQueue;
    BroadcastState broadcastState;
    TaskHandle_t xBroadcastTaskHandle;
    esp_ble_adv_params_t ble_adv_params;
    broadcast_state_changed_callback state_change_cb;
    broadcast_new_data_set_cb data_set_cb;
    SemaphoreHandle_t xMutex;
}
broadcast_control_structure;


static broadcast_control_structure bc = 
{
    .broadcastState = BROADCAST_CONTROLLER_NOT_INITIALIZED
};


static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BroadcastEvent evt = {0};
    evt.event_type = event;

    // Handle specific event types
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            ESP_LOGI(BROADCAST_LOG_GROUP, "New PDU set!");
            break;

        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(BROADCAST_LOG_GROUP, "Adv start failed: %s", esp_err_to_name(param->adv_start_cmpl.status));
            } else {
                ESP_LOGI(BROADCAST_LOG_GROUP, "Adv started!");
            }
            break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(BROADCAST_LOG_GROUP, "Adv stop failed: %s", esp_err_to_name(param->adv_stop_cmpl.status));
            }
            break;

        default:
            break;
    }

    // Enqueue event for deferred handling
    if (xQueueSendFromISR(eventQueue, &evt, &xHigherPriorityTaskWoken) != pdPASS) {
        ESP_LOGE(BROADCAST_LOG_GROUP, "Event queue full!");
    }

    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}




static void ble_appRegister(void)
{
    esp_err_t status;

    ESP_LOGI(BROADCAST_LOG_GROUP, "register callback");

    //register the scan callback function to the gap module
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK)
    {
        ESP_LOGE(BROADCAST_LOG_GROUP, "gap register error: %s", esp_err_to_name(status));
        return;
    }
}


static esp_err_t init_resources()
{
    bc.ble_adv_params = default_ble_adv_params;
    bc.broadcastState = BROADCAST_CONTROLLER_NOT_INITIALIZED;

    if (bc.xMutex == NULL) {
        bc.xMutex = xSemaphoreCreateMutex();
        if (bc.xMutex == NULL) {
            ESP_LOGE(BROADCAST_LOG_GROUP, "Failed to create mutex!");
            return ESP_ERR_NO_MEM;
        }
    }

    if (eventQueue == NULL) {
        eventQueue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(BroadcastEvent));
        if (eventQueue == NULL) {
            ESP_LOGE(BROADCAST_LOG_GROUP, "Failed to create event queue!");
            return false;
        }
    }

    bc.broadcastState = BROADCAST_CONTROLLER_INITIALIZED;
    return ESP_OK;
}


static void ble_sender_main(void) {
    BroadcastEvent evt;

    ble_appRegister();

    while (1) {
        if (xQueueReceive(eventQueue, &evt, portMAX_DELAY) == pdTRUE) {
            switch (evt.event_type) {
                case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
                    if (bc.data_set_cb) {
                        bc.data_set_cb();
                    }
                    break;

                case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
                    if (bc.state_change_cb) {
                        bc.state_change_cb(BROADCAST_CONTROLLER_BROADCASTING_RUNNING);
                    }
                    break;

                case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
                    if (bc.state_change_cb) {
                        bc.state_change_cb(BROADCAST_CONTROLLER_BROADCASTING_FORCED_STOP);
                    }
                    break;

                default:
                    ESP_LOGW(BROADCAST_LOG_GROUP, "Unhandled event type: %d", evt.event_type);
                    break;
            }
        }
    }
}




bool init_broadcasting()
{
    static bool ble_controller_initialized = false;
    if (ble_controller_initialized == false)
    {
        ble_controller_initialized = init_resources() == ESP_OK ? true: false;
    }
    return ble_controller_initialized;
}

void init_broadcast_controller()
{
    
    bool status = init_broadcasting();
    
    if (status == true)
    {
        if (xSemaphoreTake(bc.xMutex, pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
        {
            if (bc.xBroadcastTaskHandle == NULL)
            {
                BaseType_t  taskCreateResult = xTaskCreatePinnedToCore(
                    (TaskFunction_t) ble_sender_main,
                    BROADCAST_TASK_NAME, 
                    (uint32_t) BROADCAST_TASK_SIZE,
                    NULL,
                    (UBaseType_t) BROADCAST_TASK_PRIORITY,
                    (TaskHandle_t * )&(bc.xBroadcastTaskHandle),
                    BROADCAST_TASK_CORE
                    );
            
                if (taskCreateResult != pdPASS)
                {
                    bc.xBroadcastTaskHandle = NULL;
                    ESP_LOGE(BROADCAST_LOG_GROUP, "Task was not created successfully! :(");
                }
                else
                {
                    ESP_LOGI(BROADCAST_LOG_GROUP, "Task was created successfully! :)");
                }
            
            }
            else
            {
                ESP_LOGW(BROADCAST_LOG_GROUP, "Task already running!");
            }

            xSemaphoreGive(bc.xMutex);
        }
    }
}

BroadcastState get_broadcast_state()
{
    BroadcastState state = BROADCAST_CONTROLLER_NOT_INITIALIZED;
    if (xSemaphoreTake(bc.xMutex, pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
    {
        state = bc.broadcastState;
        xSemaphoreGive(bc.xMutex);
    }
    return state;
}
void register_broadcast_state_change_callback(broadcast_state_changed_callback cb)
{
    if (xSemaphoreTake(bc.xMutex, pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
    {
        bc.state_change_cb = cb;
        xSemaphoreGive(bc.xMutex);
    }
}

void register_broadcast_new_data_callback(broadcast_new_data_set_cb cb)
{
    if (xSemaphoreTake(bc.xMutex, pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
    {
        bc.data_set_cb = cb;
        xSemaphoreGive(bc.xMutex);
    }
}

void set_broadcasting_payload(uint8_t *payload, size_t payload_size)
{
    uint8_t temp_payload[MAX_GAP_DATA_LEN] = {0};
    if (payload_size <= MAX_GAP_DATA_LEN)
    {
        memcpy(temp_payload, payload, payload_size);
    } 
    else
    {
        ESP_LOGE(BROADCAST_LOG_GROUP, "Failed to set broadcast adv data");
        return;
    }
    esp_ble_gap_config_adv_data_raw(temp_payload, payload_size);
}

void set_broadcast_time_interval(uint32_t time_interval_ms)
{
    if (time_interval_ms < (ADV_INT_MIN_MS + MINUS_INTERVAL_TOLERANCE_MS) || 
        time_interval_ms > (ADV_INT_MAX_MS + PLUS_INTERVAL_TOLERANCE_MS)) {
        ESP_LOGE(BROADCAST_LOG_GROUP, "Invalid advertising interval!");
        return;
    }

    if (xSemaphoreTake(bc.xMutex, pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
    {
        bc.ble_adv_params.adv_int_min = MS_TO_N_CONVERTION(time_interval_ms - MINUS_INTERVAL_TOLERANCE_MS);
        bc.ble_adv_params.adv_int_max = MS_TO_N_CONVERTION(time_interval_ms + PLUS_INTERVAL_TOLERANCE_MS);
        xSemaphoreGive(bc.xMutex);
    }
}

void start_broadcasting()
{
    if (xSemaphoreTake(bc.xMutex, pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
    {
        esp_ble_gap_start_advertising(&default_ble_adv_params);
        xSemaphoreGive(bc.xMutex);
    }
}

void stop_broadcasting()
{
    if (xSemaphoreTake(bc.xMutex, pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
    {
        esp_ble_gap_stop_advertising();
        xSemaphoreGive(bc.xMutex);
    }
}