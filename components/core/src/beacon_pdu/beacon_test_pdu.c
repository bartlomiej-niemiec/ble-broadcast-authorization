#include "beacon_test_pdu.h"
#include <string.h>

static pdu_test_marker test_marker = {
    .marker = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
};

esp_err_t build_test_start_pdu (beacon_test_pdu *test_pdu)
{
    if (test_pdu == NULL)
        return ESP_ERR_NO_MEM;

    test_pdu->cmd.command = START_TEST;
    memcpy(test_pdu->marker.marker, test_marker.marker, sizeof(pdu_test_marker));
    return ESP_OK;
}

esp_err_t build_test_end_pdu (beacon_test_pdu *test_pdu)
{
    if (test_pdu == NULL)
        return ESP_ERR_NO_MEM;

    test_pdu->cmd.command = END_TEST;
    memcpy(test_pdu->marker.marker, test_marker.marker, sizeof(pdu_test_marker));
    return ESP_OK;
}

esp_err_t is_test_pdu(uint8_t *data, size_t data_len)
{
    if (data == NULL)
        return ESP_ERR_NO_MEM;

    bool is_equal = memcmp(data, test_marker.marker, sizeof(pdu_test_marker)) == 0 ? true : false;
    if (is_equal == true)
    {
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t is_test_start_pdu(uint8_t *data, size_t data_len)
{
    if (data == NULL)
        return ESP_ERR_NO_MEM;

    beacon_test_pdu * pdu = (beacon_test_pdu *) data;
    bool is_equal = pdu->cmd.command == START_TEST ? true : false;

    if (is_equal == true)
    {
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t is_test_end_pdu(uint8_t *data, size_t data_len)
{
    if (data == NULL)
        return ESP_ERR_NO_MEM;

    beacon_test_pdu * pdu = (beacon_test_pdu *) data;
    bool is_equal = pdu->cmd.command == END_TEST ? true : false;

    if (is_equal == true)
    {
        return ESP_OK;
    }

    return ESP_FAIL;
}

