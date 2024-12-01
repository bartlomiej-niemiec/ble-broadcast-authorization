 
#include "esp_log.h"
#include "esp_gap_ble_api.h"
#include "esp_timer.h"
#include "beacon_pdu_data.h"
#include "esp_random.h"

static const char * BLE_GAP_LOG_GROUP = "BLE_RECEIVER";

static esp_timer_handle_t timerHandle = NULL;
uint16_t broadcast_interval_ms = 50;
static int64_t timerTimeoutUs;
static beacon_pdu_data pdu_test = {
    .marker = { {0xFF, 0x8, 0x0, 0x8} },
    .bcd = {0},
    .payload = {0}
};

static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min        = 800,
    .adv_int_max        = 840,
    .adv_type           = ADV_TYPE_NONCONN_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};


void timer_callback(void *parg)
{
    esp_ble_gap_stop_advertising();
}

const esp_timer_create_args_t stTimerConfig = 
{
    .callback = timer_callback,
    .arg = NULL, 
    .dispatch_method = ESP_TIMER_TASK,
    .name = "PduSendNotificationTimer",
    .skip_unhandled_events = true
};

uint16_t get_n_for_ms(uint16_t ms)
{
    return (uint16_t) (ms / 0.625);
}

void update_ble_adv_params_t(void)
{
    uint32_t randomNum = esp_random();
    broadcast_interval_ms = ((randomNum % 20) + 1) * 100;
    ble_adv_params.adv_int_min = get_n_for_ms(broadcast_interval_ms);
    ble_adv_params.adv_int_max = get_n_for_ms(broadcast_interval_ms + 10);
    timerTimeoutUs = (int64_t) ((broadcast_interval_ms * 3) * 1000);
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
            esp_timer_start_once(timerHandle, timerTimeoutUs);
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(BLE_GAP_LOG_GROUP, "Adv stop failed: %s", esp_err_to_name(err));
        }
        else 
        {
            update_ble_adv_params_t();
            esp_ble_gap_config_adv_data_raw((uint8_t*)&pdu_test, sizeof(beacon_pdu_data));
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

 void ble_start_sender(void)
{
    esp_err_t timer_error = esp_timer_create(&stTimerConfig, &timerHandle);
    if (timer_error != ESP_OK)
    {
        ESP_LOGE(BLE_GAP_LOG_GROUP, "Config Timer failed: %s", esp_err_to_name(timer_error));
    }
    ble_appRegister();
    esp_err_t status = esp_ble_gap_config_adv_data_raw((uint8_t*)&pdu_test, sizeof(beacon_pdu_data));
    if (status == ESP_OK){
        esp_ble_gap_config_adv_data_raw((uint8_t*)&pdu_test, sizeof(beacon_pdu_data));
    }
    else {
        ESP_LOGE(BLE_GAP_LOG_GROUP, "Config iBeacon data failed: %s", esp_err_to_name(status));
    }
}