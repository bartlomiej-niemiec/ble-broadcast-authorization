/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */


/****************************************************************************
*
* This file is for iBeacon definitions. It supports both iBeacon sender and receiver
* which is distinguished by macros IBEACON_SENDER and IBEACON_RECEIVER,
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
#include "esp_gattc_api.h"

#define IBEACON_SENDER      0
#define IBEACON_RECEIVER    1
#define IBEACON_MODE CONFIG_IBEACON_MODE

/* Major and Minor part are stored in big endian mode in iBeacon packet,
 * need to use this macro to transfer while creating or processing
 * iBeacon data */
#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00)>>8) + (((x)&0xFF)<<8))

typedef struct {
    uint8_t flags[3];
    uint8_t length;
    uint8_t type;
    uint16_t company_id;
    uint16_t beacon_type;
}__attribute__((packed)) esp_ble_ibeacon_head_t;

typedef struct {
    uint8_t msg[16];
}__attribute__((packed)) esp_ble_ibeacon_hello_world_msg;

typedef struct {
    uint16_t packetTimeArrivalMs;
}__attribute__((packed)) esp_ble_ibeacon_pdu_arrival_ms;

typedef struct {
    esp_ble_ibeacon_head_t ibeacon_head;
    esp_ble_ibeacon_pdu_arrival_ms pdu_arrival_ms;
    esp_ble_ibeacon_hello_world_msg ibeacon_hello;
}__attribute__((packed)) esp_ble_ibeacon_t;



/* For iBeacon packet format, please refer to Apple "Proximity Beacon Specification" doc */
/* Constant part of iBeacon data */
extern esp_ble_ibeacon_head_t ibeacon_common_head;

bool esp_ble_is_ibeacon_packet (uint8_t *adv_data, uint8_t adv_data_len);

esp_err_t esp_ble_config_ibeacon_data (esp_ble_ibeacon_hello_world_msg *hello_world, esp_ble_ibeacon_pdu_arrival_ms * pdu_arrival_st, esp_ble_ibeacon_t *ibeacon_adv_data);
