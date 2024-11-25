#include "beacon_pdu_data.h"
#include <string.h>

beacon_marker my_marker = {
    .marker = {0xFF, 0x8, 0x0, 0x8}
};

esp_err_t build_beacon_pdu_data (beacon_crypto_data *crypto, uint8_t* payload, size_t payload_size, beacon_pdu_data *bpd)
{
    if ((crypto == NULL) || (payload == NULL || payload_size > MAX_PDU_PAYLOAD_SIZE) || (bpd == NULL)){
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&(bpd->marker), &my_marker, sizeof(beacon_marker));
    memcpy(&(bpd->bcd), crypto, sizeof(beacon_crypto_data));
    memcpy(&(bpd->payload), payload, payload_size);

    return ESP_OK;
}
