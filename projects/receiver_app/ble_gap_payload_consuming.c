#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include <string.h>
#include <stdint.h>


void payload_notifier(uint8_t *data, size_t data_len, esp_bd_addr_t mac_address)
{
    if (data != NULL && mac_address != NULL)
    {
        uint32_t val;
        memcpy(&val, data, sizeof(uint32_t));
        ESP_LOGI("PAYLOAD_NOTIFIER", "Payload size: %i", (int) data_len);
        ESP_LOGI("PAYLOAD_NOTIFIER", "Payload uint32_t: %lu", val);
    }
}
