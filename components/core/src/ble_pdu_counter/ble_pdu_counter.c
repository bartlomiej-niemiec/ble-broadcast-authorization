#include "ble_pdu_counter.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_log.h"

uint32_t pdu_counter = 0;

static const char * PDU_COUNTER_LOG = "PDU COUNTER";

void packet_has_been_send()
{
}

int packet_has_been_received(uint8_t *data, uint16_t len)
{
    ESP_LOGI(PDU_COUNTER_LOG, "Received packet from Controller: Length=%d", len);

    // Analyze the HCI event
    uint8_t event_code = data[0]; // HCI event code
    if (event_code == 0x3E) { // HCI_LE_META_EVENT
        uint8_t sub_event_code = data[2]; // Sub-event code
        if (sub_event_code == 0x02) { // HCI_LE_ADVERTISING_REPORT_EVENT
            pdu_counter++;
            ESP_LOGI(PDU_COUNTER_LOG, "Advertising PDU sent. Total PDUs: %lu", pdu_counter);
        }
    }

    return 0; // Return 0 if the packet is successfully processed
}

static esp_vhci_host_callback_t my_callbacks = {
    packet_has_been_send,
    packet_has_been_received
};

// HCI callback function
esp_err_t init_pdu_counter()
{
    static bool is_pdu_counter_initialized = false;
    if (!is_pdu_counter_initialized)
    {
        esp_vhci_host_register_callback(&my_callbacks);
        if (!esp_vhci_host_check_send_available()) {
        ESP_LOGW(PDU_COUNTER_LOG, "VHCI host not ready to send");
            return ESP_FAIL;
        }
        ESP_LOGI(PDU_COUNTER_LOG, "VHCI host initialized");
        is_pdu_counter_initialized = true;
    }
    return ESP_OK;
}

uint32_t get_pdu_counter()
{
    return pdu_counter;
}