//My libs
#include "dispatcher.h"
#include "beacon_pdu_data.h"
#include "sec_pdu_processing.h"

//Esp32
#include "esp_log.h"

//System
#include <string.h>

static const char* DIST_LOG_GROUP = "PDU DISPATCHER";


bool create_ble_broadcast_pdu_for_dispatcher(ble_broadcast_pdu* pdu, uint8_t *data, size_t size, esp_bd_addr_t mac_address)
{
    bool result = false;
    if (size <= MAX_BLE_BROADCAST_SIZE_BYTES)
    {
        memcpy(pdu->data, data, size);
        memcpy(&(pdu->data_len), &size, sizeof(size));
        memcpy(&(pdu->mac_address), mac_address, sizeof(esp_bd_addr_t));
        result = true;
    }
    else
    {
        ESP_LOGE(DIST_LOG_GROUP, "Size of data larger than pdu->data!");
    }
    return result;
}

void dispatch_pdu(ble_broadcast_pdu* pdu, esp_bd_addr_t mac_address)
{
    if (pdu != NULL) {
        ble_broadcast_pdu* pdu_struct = (ble_broadcast_pdu *) pdu;

        if (true == is_pdu_in_beacon_pdu_format(pdu_struct->data, pdu_struct->data_len))
        {
            enqueue_pdu_for_processing((beacon_pdu_data *) pdu_struct->data, mac_address);
        }
    }
}







