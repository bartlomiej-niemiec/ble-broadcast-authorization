#include "ble_broadcast.h"

#include "esp_log.h"
#include "esp_gap_ble_api.h"
#include "esp_timer.h"
#include "esp_random.h"

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


typedef struct {
    bool isBroadcastingRunning;
    QueueHandle_t pduQueue;
    TaskHandle_t xBroadcastTaskHandle;
    esp_ble_adv_params_t ble_adv_params;
}
broadcast_control;


static volatile broadcast_control bc;
static SemaphoreHandle_t bc_mutex = NULL;
static bool isBroadcastingInitialized = false;

BaseType_t queue_pdu_for_broadcast(beacon_pdu_data *pdu)
{
    BaseType_t stats = pdFAIL;
    if (pdu != NULL)
    {
        stats = xQueueSend(bc.pduQueue, ( void * ) pdu, ( TickType_t ) 0 );
    }
    return stats;
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{ 
    esp_err_t err;

    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        //ESP_LOGI(BROADCAST_LOG_GROUP, "New payload has been set for advertising");
        if (bc.isBroadcastingRunning == false)
        {
            esp_ble_gap_start_advertising((esp_ble_adv_params_t *)&(bc.ble_adv_params));
        }
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //adv start complete event to indicate adv start successfully or failed
        if ((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(BROADCAST_LOG_GROUP, "Adv start failed: %s", esp_err_to_name(err));
        }
        else{
            ESP_LOGI(BROADCAST_LOG_GROUP, "Start Broadcasting");
            bc.isBroadcastingRunning = true;
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(BROADCAST_LOG_GROUP, "Adv stop failed: %s", esp_err_to_name(err));
        }
        else 
        {
            bc.isBroadcastingRunning = false;
            ESP_LOGI(BROADCAST_LOG_GROUP, "Stop Broadcasting");
        }
        break;
    default:
        break;
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
    bc.isBroadcastingRunning = false;
    bc.pduQueue = xQueueCreate(MAX_ELEMENTS_IN_QUEUE, sizeof(beacon_pdu_data));

    if (bc.pduQueue == NULL)
    {
        ESP_LOGE(BROADCAST_LOG_GROUP, "Failed to create PDU Queue!");
        return ESP_ERR_NO_MEM;
    }

    if (bc_mutex == NULL)
    {
        bc_mutex = xSemaphoreCreateMutex();
        if (bc_mutex == NULL)
        {
            ESP_LOGE(BROADCAST_LOG_GROUP, "Failed to create mutex!");
            abort();
        }
    }

    return ESP_OK;
}

static void set_new_adv_data(beacon_pdu_data *pdu)
{
    esp_err_t err = esp_ble_gap_config_adv_data_raw((uint8_t*) pdu, sizeof(beacon_pdu_data));
    if (err != ESP_OK) {
        ESP_LOGE(BROADCAST_LOG_GROUP, "Failed to set advertisement data: %s", esp_err_to_name(err));
    }
}

static void ble_sender_main(void)
{
    ble_appRegister();

    beacon_pdu_data pdu;

    const int taskDelayTicks = pdMS_TO_TICKS(50);

    while (1)
    {  
        if (xQueueReceive(bc.pduQueue, ( void * ) &pdu, QUEUE_TIMEOUT_MS / portTICK_PERIOD_MS) == pdTRUE)
        {
            set_new_adv_data(&pdu);
        }
        vTaskDelay(taskDelayTicks);
    };
}

void init_broadcasting()
{
    if (isBroadcastingInitialized == false)
    {
        isBroadcastingInitialized = init_resources() == ESP_OK ? true: false;
    }
}

void start_up_broadcasting()
{
    
    init_broadcasting();
    
    if (isBroadcastingInitialized == true)
    {
        if (xSemaphoreTake(bc_mutex, pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
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

            xSemaphoreGive(bc_mutex);
        }
    }
}

void stop_broadcasting()
{
    if (xSemaphoreTake(bc_mutex, 0) == pdTRUE)
    {
        if (bc.xBroadcastTaskHandle != NULL)
        {
            vTaskDelete(bc.xBroadcastTaskHandle);
            bc.isBroadcastingRunning = false;
            bc.xBroadcastTaskHandle = NULL;

            //empty whole queue
            beacon_pdu_data pdu;
            while (xQueueReceive(bc.pduQueue, ( void * ) &pdu, QUEUE_TIMEOUT_MS / portTICK_PERIOD_MS) == pdTRUE);
        }
        xSemaphoreGive(bc_mutex);
    }
}

bool is_broadcasting_running()
{
    bool is_running = false;
    if (xSemaphoreTake(bc_mutex, 0) == pdTRUE)
    {
        is_running = bc.xBroadcastTaskHandle != NULL ? true : false;
        xSemaphoreGive(bc_mutex);
    }
    return is_running;
}