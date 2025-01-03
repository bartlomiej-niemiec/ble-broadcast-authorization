#include "ble_broadcast_controller.h"
#include "beacon_pdu_data.h"

#include "esp_log.h"
#include "esp_gap_ble_api.h"
#include "esp_timer.h"
#include "esp_random.h"
#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>

#define EVENT_QUEUE_SIZE 30
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

typedef struct {
    esp_gap_ble_cb_event_t event_type;
    int64_t timestamp;
    esp_bd_addr_t mac_addr;
    uint8_t payload[MAX_GAP_DATA_LEN];
    size_t payload_size;
} BroadcastEvent;

typedef struct {
    QueueHandle_t eventQueue;
    atomic_int   broadcastState;
    atomic_int   scannerState;
    TaskHandle_t xBroadcastTaskHandle;
    esp_ble_adv_params_t ble_adv_params;
    broadcast_state_changed_callback state_change_cb;
    broadcast_new_data_set_cb data_set_cb;
    scan_complete scan_complete_cb;
    SemaphoreHandle_t xMutex;
} broadcast_control_structure;


static broadcast_control_structure bc = 
{
    .broadcastState = BROADCAST_CONTROLLER_BROADCASTING_NOT_RUNNING,
    .scannerState = SCANNER_CONTROLLER_SCANNING_NOT_ACTIVE,
    .state_change_cb = NULL,
    .data_set_cb = NULL,
    .scan_complete_cb = NULL
};

void set_broadcast_state(BroadcastState state);
void set_scanner_state(ScannerState state);


static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    // Prepare event structure
    BroadcastEvent evt = { .event_type = event, .payload_size = 0, .payload = {0}, .timestamp = 0 };    
    // // Handle specific event types
    if (event == ESP_GAP_BLE_SCAN_RESULT_EVT) {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        if (scan_result->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
            evt.timestamp = esp_timer_get_time();
            // Safely handle payload size
            if (scan_result->scan_rst.adv_data_len <= MAX_GAP_DATA_LEN) {
                evt.payload_size = scan_result->scan_rst.adv_data_len;
                memcpy(evt.mac_addr, scan_result->scan_rst.bda, sizeof(esp_bd_addr_t));
                memcpy(evt.payload, scan_result->scan_rst.ble_adv, evt.payload_size);
            } else {
                ESP_LOGW(BROADCAST_LOG_GROUP, "Adv data too large: %d bytes", scan_result->scan_rst.adv_data_len);
            }
        }
    }

    // Attempt to send event to task queue
    if (xQueueSend(bc.eventQueue, &evt, pdMS_TO_TICKS(QUEUE_TIMEOUT_MS)) != pdPASS) {
        ESP_LOGW(BROADCAST_LOG_GROUP, "Event queue full! Dropping event type: %d", event);
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

    if (bc.xMutex == NULL) {
        bc.xMutex = xSemaphoreCreateMutex();
        if (bc.xMutex == NULL) {
            ESP_LOGE(BROADCAST_LOG_GROUP, "Failed to create mutex!");
            return ESP_ERR_NO_MEM;
        }
    }

    if (bc.eventQueue == NULL) {
        bc.eventQueue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(BroadcastEvent));
        if (bc.eventQueue == NULL) {
            ESP_LOGE(BROADCAST_LOG_GROUP, "Failed to create event queue!");
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_LOGE(BROADCAST_LOG_GROUP, "Initialization succed!");

    return ESP_OK;
}


static void ble_sender_main(void) {
    BroadcastEvent evt;

    while (1) {
        // Receive events with a timeout for periodic maintenance
        if (xQueueReceive(bc.eventQueue, &evt, pdMS_TO_TICKS(500)) == pdTRUE) {
            switch (evt.event_type) {
                case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
                    if (bc.data_set_cb) {
                        ESP_LOGE(BROADCAST_LOG_GROUP, "New Data Set!");
                        bc.data_set_cb();
                    }
                    break;

                case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
                    set_broadcast_state(BROADCAST_CONTROLLER_BROADCASTING_RUNNING);
                    if (bc.state_change_cb) {
                        bc.state_change_cb(BROADCAST_CONTROLLER_BROADCASTING_RUNNING);
                    }
                    break;


                case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
                    set_broadcast_state(BROADCAST_CONTROLLER_BROADCASTING_NOT_RUNNING);
                    if (bc.state_change_cb) {
                        bc.state_change_cb(BROADCAST_CONTROLLER_BROADCASTING_NOT_RUNNING);
                    }
                    break;

                case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
                    set_scanner_state(SCANNER_CONTROLLER_SCANNING_ACTIVE);
                    break;

                case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
                    set_scanner_state(SCANNER_CONTROLLER_SCANNING_NOT_ACTIVE);
                    break;

                case ESP_GAP_BLE_SCAN_RESULT_EVT:
                    if (bc.scan_complete_cb) {
                        bc.scan_complete_cb(evt.timestamp, evt.payload, evt.payload_size, evt.mac_addr);
                    }
                    break;

                default:
                    ESP_LOGD(BROADCAST_LOG_GROUP, "Unhandled event type: %d", evt.event_type);
                    break;
            }
        } else {
            // Perform periodic maintenance here
            ESP_LOGI(BROADCAST_LOG_GROUP, "Task is alive, monitoring...");
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

bool init_broadcast_controller()
{
    
    bool status = init_broadcasting();
    
    if (status == true)
    {
        if (xSemaphoreTake(bc.xMutex, pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
        {
            ble_appRegister();
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
                    return false;
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

    return true;
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

void register_scan_complete_callback(scan_complete cb)
{
    if (xSemaphoreTake(bc.xMutex, pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
    {
        bc.scan_complete_cb = cb;
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

void stop_broadcasting()
{
    if (get_broadcast_state() == BROADCAST_CONTROLLER_BROADCASTING_RUNNING)
    {
        esp_ble_gap_stop_advertising();
    }
}

void start_broadcasting() {
    if (get_scanner_state() == SCANNER_CONTROLLER_SCANNING_ACTIVE) {
        ESP_LOGE(BROADCAST_LOG_GROUP, "Stopping Broadcasting");
        stop_scanning(); // Ensure no overlap
    }
    if (get_broadcast_state() == BROADCAST_CONTROLLER_BROADCASTING_NOT_RUNNING) {
        ESP_LOGE(BROADCAST_LOG_GROUP, "Starting Broadcasting");
        esp_ble_gap_start_advertising(&default_ble_adv_params);
        set_broadcast_state(BROADCAST_CONTROLLER_BROADCASTING_RUNNING);
    }
}

void start_scanning(esp_ble_scan_params_t scan_params, uint32_t scan_duration_s) {
    if (get_broadcast_state() == BROADCAST_CONTROLLER_BROADCASTING_RUNNING) {
        stop_broadcasting(); // Ensure no overlap
    }
    if (get_scanner_state() == SCANNER_CONTROLLER_SCANNING_NOT_ACTIVE) {
        esp_ble_gap_set_scan_params(&scan_params);
        esp_ble_gap_start_scanning(scan_duration_s);
        set_scanner_state(SCANNER_CONTROLLER_SCANNING_ACTIVE);
    }
}


void stop_scanning()
{
    if (get_scanner_state() == SCANNER_CONTROLLER_SCANNING_ACTIVE)
    {
        esp_ble_gap_stop_scanning();
    }
}

BroadcastState get_broadcast_state()
{
    return atomic_load(&bc.broadcastState);
}

ScannerState get_scanner_state()
{
    return atomic_load(&bc.scannerState);
}

void set_broadcast_state(BroadcastState state)
{
    atomic_store(&bc.broadcastState, state);
}

void set_scanner_state(ScannerState state)
{
    atomic_store(&bc.scannerState, state);
}