#include "beacon_pdu_data.h"
#include <string.h>
#include "esp_log.h"

static const char* BEACON_PDU_GROUP = "BEACON_PDU_GROUP";

beacon_marker my_marker = {
    .marker = {0xFF, 0x8, 0x0}
};

beacon_pdu_data keep_alive_pdu = {0};


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

bool is_pdu_in_beacon_pdu_format(uint8_t *data, size_t size) 
{
    bool is_pdu_beacon_format = false;

    if (size < sizeof(beacon_marker))
    {
        ESP_LOGE(BEACON_PDU_GROUP, "PDU size is lower than marker");
    }
    else
    {
        is_pdu_beacon_format = memcmp(data, &my_marker, sizeof(beacon_marker)) == 0;
    }

    return is_pdu_beacon_format;
}

esp_err_t fill_marker_in_pdu(beacon_pdu_data *bpd)
{
    if ((bpd == NULL)){
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&(bpd->marker), &my_marker, sizeof(beacon_marker));
    return ESP_OK;
}