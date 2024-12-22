#include "esp_gap_ble_api.h"
#include "esp_log.h"

#include <stdint.h>


void payload_notifier(uint8_t *data, size_t data_len, esp_bd_addr_t mac_address)
{
    if (data != NULL && mac_address != NULL)
    {
        ESP_LOG_BUFFER_HEX("Received packet from:", mac_address, sizeof(esp_bd_addr_t));
        ESP_LOGI("SEC PAYLOAD CONSUMING", "Payload: %s", (char *)data);
 
    }
}
