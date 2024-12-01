 
#include "esp_log.h"
#include "esp_gap_ble_api.h"
#include "esp_timer.h"
#include "beacon_pdu_data.h"
#include "dispatcher.h"

static const char * BLE_GAP_LOG_GROUP = "BLE_RECEIVER";

inline uint64_t get_timestamp_ms()
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static esp_ble_scan_params_t default_ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 160,
    .scan_window            = 160,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    switch (event) {

    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        //the unit of the duration is second, 0 means scan permanently
        uint32_t duration = 0;
        esp_ble_gap_start_scanning(duration);
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        //scan start complete event to indicate scan start successfully or failed
        if ((err = param->scan_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(BLE_GAP_LOG_GROUP, "Scan start failed: %s", esp_err_to_name(err));
        }
        break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        uint64_t timestamp = get_timestamp_ms();
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            {
                if (is_pdu_in_beacon_pdu_format(scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len))
                {
                    ble_broadcast_pdu pdu = {0};
                    create_ble_broadcast_pdu_for_dispatcher(&pdu, scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len, timestamp);
                    AddPduToDispatcher(&pdu);
                }
            }
            break;
        default:
            break;
        }
    }
    break;

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if ((err = param->scan_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(BLE_GAP_LOG_GROUP, "Scan stop failed: %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGI(BLE_GAP_LOG_GROUP, "Stop scan successfully");
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
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        ESP_LOGE(BLE_GAP_LOG_GROUP, "gap register error: %s", esp_err_to_name(status));
        return;
    }

}

 void ble_start_receiver(void)
{
    SpawnDispatcherTask();
    ble_appRegister();
    esp_ble_gap_set_scan_params(&default_ble_scan_params);
}