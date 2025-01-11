 
#include "esp_log.h"
#include "esp_gap_ble_api.h"
#include "beacon_pdu_data.h"
#include "ble_gap_payload_consuming.h"
#include "sec_pdu_processing.h"
#include "ble_broadcast_controller.h"
#include "pc_serial_communication.h"
#include "receiver_serial_communication.h"

static const char * BLE_GAP_LOG_GROUP = "BLE_RECEIVER";

static esp_ble_scan_params_t default_ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 160,
    .scan_window            = 160,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

void app_main(void)
{
    int sec_pdu_status = start_up_sec_processing();
    if (sec_pdu_status == 0)
    {
        register_payload_observer_cb(payload_notifier);
        ESP_LOGI(BLE_GAP_LOG_GROUP, "Sec PDU Creation Success");
    }
    else
    {
        ESP_LOGI(BLE_GAP_LOG_GROUP, "Sec PDU Creation Failed: %i", sec_pdu_status);
        return;
    }

    if (start_pc_serial_communication() != ESP_OK)
    {
        ESP_LOGE(BLE_GAP_LOG_GROUP, "Failed initializtion of pc communication");
        return;
    }
    
    register_serial_data_received_cb(serial_data_received);

    bool init_stat = init_broadcast_controller();
    if (init_stat == true)
    {
        register_scan_complete_callback(scan_complete_callback);
        start_scanning(default_ble_scan_params, 0);
    }
    else
    {
        ESP_LOGE(BLE_GAP_LOG_GROUP, "Failed initializtion of broadcast controller");
    }
}