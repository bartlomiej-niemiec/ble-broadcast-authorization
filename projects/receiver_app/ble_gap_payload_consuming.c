#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "sec_pdu_processing.h"
#include "beacon_pdu_helpers.h"

#include <stdint.h>


void payload_notifier(uint8_t *data, size_t data_len, esp_bd_addr_t mac_address)
{
    if (data != NULL && mac_address != NULL)
    {
        ESP_LOG_BUFFER_HEX("Received packet from:", mac_address, sizeof(esp_bd_addr_t));
        ESP_LOGI("SEC PAYLOAD CONSUMING", "Payload: %s", (char *)data);
 
    }
}

void receiver_app_scan_complete_cb(int64_t timestamp_us, uint8_t *data, size_t data_size, esp_bd_addr_t mac_address)
{
    if (data != NULL && mac_address != NULL)
    {
        if (is_pdu_in_beacon_pdu_format(data, data_size) == true)
        {
            enqueue_pdu_for_processing(timestamp_us, data, data_size, mac_address);
        }
    }
}