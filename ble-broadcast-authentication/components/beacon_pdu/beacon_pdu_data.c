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

void build_nonce(uint8_t nonce[NONCE_SIZE], const beacon_marker* marker, uint8_t key_fragment_number, uint16_t key_id, uint8_t xor_seed)
{
    memset(nonce, 0, 16);

    // Copy the marker into the beginning of the nonce.
    memcpy(nonce, marker->marker, sizeof(beacon_marker));

    // Add the key ID (2 bytes) in little-endian format.
    nonce[sizeof(beacon_marker)]     = key_id & 0xFF;        // Low byte of key ID
    nonce[sizeof(beacon_marker) + 1] = (key_id >> 8) & 0xFF; // High byte of key ID

    // Add the key fragment number (1 byte).
    nonce[sizeof(beacon_marker) + 2] = key_fragment_number;

    // Add the XOR seed (1 byte).
    nonce[sizeof(beacon_marker) + 3] = xor_seed;

    // The remaining bytes of the nonce are already zero-filled from memset.
}