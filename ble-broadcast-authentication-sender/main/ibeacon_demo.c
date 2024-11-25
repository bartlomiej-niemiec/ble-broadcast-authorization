/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */



/****************************************************************************
*
* This file is for iBeacon demo. It supports both iBeacon sender and receiver
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
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_ibeacon_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include "esp_random.h"

static const char* DEMO_TAG = "IBEACON_DEMO";
char msg[16] = {};
esp_ble_ibeacon_hello_world_msg hello_world_msg;
esp_ble_ibeacon_pdu_arrival_ms pdu_arrival_ms;
static esp_timer_handle_t timerHandle = NULL;
static int64_t timerTimeoutUs;
esp_ble_ibeacon_t ibeacon_adv_data;


uint16_t broadcast_interval_ms = 50;

///Declare static functions
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void ble_ibeacon_appRegister(void);
void ble_ibeacon_init(void);
void update_ble_adv_params_t(void);
void timer_callback(void *arg);
void init_msg(void)
{
    const char* msg = "HELLO WORLD!";
    memset(hello_world_msg.msg, 0, sizeof(hello_world_msg.msg));
    memcpy(hello_world_msg.msg, msg, sizeof("HELLO WORLD!"));
}

void set_pdu_arrival_st(uint16_t nextPduArrivalMs)
{
    pdu_arrival_ms.packetTimeArrivalMs = nextPduArrivalMs;
}

void encrypt_hello_msg(int interval_ms)
{
    uint16_t size = sizeof(hello_world_msg.msg);
    int shift = interval_ms % size;
    if (shift < 0) {
        shift += size; // Handle negative shift
    }
    uint8_t tempBuffer[16];

    // Perform the rotation
    for (size_t i = 0; i < size; i++) {
        tempBuffer[i] = hello_world_msg.msg[(i + shift) % size]; // Rotate right by 'shift'
    }
    ESP_LOG_BUFFER_HEXDUMP("Pre Encryption", hello_world_msg.msg, size, ESP_LOG_INFO);
    memcpy(hello_world_msg.msg, tempBuffer, size);
    ESP_LOG_BUFFER_HEXDUMP("Post Encryption", hello_world_msg.msg, size, ESP_LOG_INFO);
}

uint16_t get_n_for_ms(uint16_t ms)
{
    return (uint16_t) (ms / 0.625);
}

static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min        = 800,
    .adv_int_max        = 840,
    .adv_type           = ADV_TYPE_NONCONN_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

const esp_timer_create_args_t stTimerConfig = 
{
    .callback = timer_callback,
    .arg = NULL, 
    .dispatch_method = ESP_TIMER_ISR,
    .name = "PduSendNotificationTimer",
    .skip_unhandled_events = true
};


void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    ble_ibeacon_init();

    esp_err_t timer_error = esp_timer_create(&stTimerConfig, &timerHandle);
    if (timer_error != ESP_OK)
    {
        ESP_LOGE(DEMO_TAG, "Config Timer failed: %s", esp_err_to_name(timer_error));
    }

    init_msg();
    esp_err_t status = esp_ble_config_ibeacon_data (&hello_world_msg, &pdu_arrival_ms, &ibeacon_adv_data);
    if (status == ESP_OK){
        esp_ble_gap_config_adv_data_raw((uint8_t*)&ibeacon_adv_data, sizeof(ibeacon_adv_data));
    }
    else {
        ESP_LOGE(DEMO_TAG, "Config iBeacon data failed: %s", esp_err_to_name(status));
    }
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&ble_adv_params);
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //adv start complete event to indicate adv start successfully or failed
        if ((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(DEMO_TAG, "Adv start failed: %s", esp_err_to_name(err));
        }
        else
        {
            esp_timer_start_once(timerHandle, timerTimeoutUs);
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(DEMO_TAG, "Adv stop failed: %s", esp_err_to_name(err));
        }
        else 
        {
            ESP_LOGI(DEMO_TAG, "Stop adv successfully");
            update_ble_adv_params_t();
            init_msg();
            encrypt_hello_msg((int) (timerTimeoutUs / 3000));
            set_pdu_arrival_st((uint16_t) (timerTimeoutUs / 3000));
            ESP_LOGE(DEMO_TAG, "Next packet should be deliver in: %i ms", pdu_arrival_ms.packetTimeArrivalMs);
            esp_ble_config_ibeacon_data (&hello_world_msg, &pdu_arrival_ms, &ibeacon_adv_data);
            esp_ble_gap_config_adv_data_raw((uint8_t*)&ibeacon_adv_data, sizeof(ibeacon_adv_data));
        }
        break;
    default:
        break;
    }
}


void ble_ibeacon_appRegister(void)
{
    esp_err_t status;

    ESP_LOGI(DEMO_TAG, "register callback");

    //register the scan callback function to the gap module
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK)
    {
        ESP_LOGE(DEMO_TAG, "gap register error: %s", esp_err_to_name(status));
        return;
    }
}

void ble_ibeacon_init(void)
{
    esp_bluedroid_init();
    esp_bluedroid_enable();
    ble_ibeacon_appRegister();
}

void timer_callback(void *parg)
{
    esp_ble_gap_stop_advertising();
}

void update_ble_adv_params_t(void)
{
    uint32_t randomNum = esp_random();
    broadcast_interval_ms = ((randomNum % 20) + 1) * 100;
    ble_adv_params.adv_int_min = get_n_for_ms(broadcast_interval_ms);
    ble_adv_params.adv_int_max = get_n_for_ms(broadcast_interval_ms + 10);
    timerTimeoutUs = (int64_t) ((broadcast_interval_ms * 3) * 1000);
}
