#include "ble_sender.h" 
#include "esp_log.h"
#include "esp_gap_ble_api.h"
#include "esp_timer.h"
#include "beacon_pdu_data.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>

#define TIME_INTERVAL_BASE_MS 100
#define MAX_BROADCAST_TIME_INTERVAL_MS 2000
#define MINUS_INTERVAL_TOLERANCE_MS 10
#define PLUS_INTERVAL_TOLERANCE_MS 10
#define MAX_ELEMENTS_IN_QUEUE 10

static const char * BLE_GAP_LOG_GROUP = "BLE_RECEIVER";

static esp_timer_handle_t pduTimerHandle = NULL;
static int64_t timerTimeoutUs;
QueueHandle_t pduQueue = NULL;
SemaphoreHandle_t broadcastInRunningSem;

static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min        = 800,
    .adv_int_max        = 840,
    .adv_type           = ADV_TYPE_NONCONN_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

void pdu_timer_callback(void *parg)
{
    esp_ble_gap_stop_advertising();
}

const esp_timer_create_args_t stPduTimerConfig = 
{
    .callback = pdu_timer_callback,
    .arg = NULL, 
    .dispatch_method = ESP_TIMER_TASK,
    .name = "PduSendNotificationTimer",
    .skip_unhandled_events = true
};

uint16_t get_n_for_ms(uint16_t ms)
{
    return (uint16_t) (ms / 0.625);
}

void update_ble_adv_params_t(esp_ble_adv_params_t * adv_params, uint16_t interval_ms, uint16_t plus_tolerance_ms, uint16_t minus_tolerance_ms)
{
    adv_params->adv_int_min = get_n_for_ms(interval_ms - minus_tolerance_ms);
    adv_params->adv_int_max = get_n_for_ms(interval_ms + plus_tolerance_ms);
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{ 
    esp_err_t err;

    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&ble_adv_params);
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //adv start complete event to indicate adv start successfully or failed
        if ((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(BLE_GAP_LOG_GROUP, "Adv start failed: %s", esp_err_to_name(err));
        }
        else
        {
            esp_timer_start_once(pduTimerHandle, timerTimeoutUs);
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(BLE_GAP_LOG_GROUP, "Adv stop failed: %s", esp_err_to_name(err));
        }
        else 
        {
            xSemaphoreGive(broadcastInRunningSem);
        }
        break;
    default:
        break;
    }
}


void ble_appRegister(void)
{
    esp_err_t status;

    ESP_LOGI(BLE_GAP_LOG_GROUP, "register callback");

    //register the scan callback function to the gap module
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK)
    {
        ESP_LOGE(BLE_GAP_LOG_GROUP, "gap register error: %s", esp_err_to_name(status));
        return;
    }
}


void ble_sender_main(void)
{
    beacon_pdu_with_time_interval pdu;

    pduQueue = xQueueCreate(MAX_ELEMENTS_IN_QUEUE, sizeof(beacon_pdu_data));

    if (pduQueue == NULL)
    {
        ESP_LOGE(BLE_GAP_LOG_GROUP, "Failed to create PDU Queue!");
    }

    broadcastInRunningSem = xSemaphoreCreateBinary();

    if (broadcastInRunningSem == NULL)
    {
        ESP_LOGE(BLE_GAP_LOG_GROUP, "Failed to create Binary Semaphore!");
    }
    else
    {
        xSemaphoreGive(broadcastInRunningSem);
    }

    uint16_t broadcast_time_interval_ms = 0;

    while (1)
    {
        if (xQueueReceive(pduQueue, ( void * ) &pdu, 0))
        {   
            if (xSemaphoreTake(broadcastInRunningSem, 0) == pdTRUE )
            {
                update_ble_adv_params_t(&ble_adv_params, pdu.time_interval_ms, PLUS_INTERVAL_TOLERANCE_MS, MINUS_INTERVAL_TOLERANCE_MS);
                timerTimeoutUs = pdu.time_interval_ms * 1000;
                esp_ble_gap_config_adv_data_raw((uint8_t*)&(pdu.pdu), sizeof(beacon_pdu_data));
            }

        }
    }

}

 void ble_start_sender(void)
{
    esp_err_t timer_error = esp_timer_create(&stPduTimerConfig, &pduTimerHandle);
    if (timer_error != ESP_OK)
    {
        ESP_LOGE(BLE_GAP_LOG_GROUP, "Config Timer failed: %s", esp_err_to_name(timer_error));
    }

    ble_appRegister();
    ble_sender_main();
}