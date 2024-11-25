/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */



/****************************************************************************
*
* This file is for iBeacon APIs. It supports both iBeacon encode and decode.
*
* iBeacon is a trademark of Apple Inc. Before building devices which use iBeacon technology,
* visit https://developer.apple.com/ibeacon/ to obtain a license.
*
****************************************************************************/

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "esp_gap_ble_api.h"
#include "esp_ibeacon_api.h"


/* For iBeacon packet format, please refer to Apple "Proximity Beacon Specification" doc */
/* Constant part of iBeacon data */
esp_ble_ibeacon_head_t ibeacon_common_head = {
    .flags = {0x02, 0x01, 0x06},
    .length = 0x1A,
    .type = 0xFF,
    .company_id = 0x004C,
    .beacon_type = 0x1502
};

bool esp_ble_is_ibeacon_packet (uint8_t *adv_data, uint8_t adv_data_len){
    bool result = false;

    if ((adv_data != NULL) && (adv_data_len == 0x1E)){
        if (!memcmp(adv_data, (uint8_t*)&ibeacon_common_head, sizeof(ibeacon_common_head))){
            result = true;
        }

    }

    return result;
}

esp_err_t esp_ble_config_ibeacon_data (esp_ble_ibeacon_hello_world_msg *hello_world, esp_ble_ibeacon_pdu_arrival_ms * pdu_arrival_st, esp_ble_ibeacon_t *ibeacon_adv_data){
    if ((hello_world == NULL) || (ibeacon_adv_data == NULL) || (pdu_arrival_st == NULL)){
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&ibeacon_adv_data->ibeacon_head, &ibeacon_common_head, sizeof(esp_ble_ibeacon_head_t));
    memcpy(&ibeacon_adv_data->pdu_arrival_ms, pdu_arrival_st, sizeof(esp_ble_ibeacon_pdu_arrival_ms));
    memcpy(&ibeacon_adv_data->ibeacon_hello, hello_world, sizeof(esp_ble_ibeacon_hello_world_msg));

    return ESP_OK;
}
