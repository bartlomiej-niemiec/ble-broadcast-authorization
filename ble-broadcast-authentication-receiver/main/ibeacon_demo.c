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
#include <math.h> 
#include "key_reconstructor.h"

static const char* DEMO_TAG = "IBEACON_DEMO";
int64_t prev_time_ms = 0;
int64_t timeElapsedInMS = 0;
int64_t delay_ms = 0;
static bool firstPacketDetected = false;

void decrypt_hello_msg(int interval_ms, uint8_t* buffer, size_t size)
{
    int shift = interval_ms % size;
    if (shift < 0) {
        shift += size; // Handle negative shift
    }
    uint8_t tempBuffer[16];

    // Perform the rotation
    for (size_t i = 0; i < size; i++) {
        tempBuffer[i] = buffer[(i + shift) % size]; // Rotate right by 'shift'
    }

    memcpy(buffer, tempBuffer, size);
}

int64_t get_round_up_elapsed_time()
{
    // Divide the time by 50 and round it to the nearest integer
    int rounded_time = round((float)timeElapsedInMS / 100.0) * 100;
    return rounded_time;
}

void logTimeSinceLastPacketReceived()
{
    int64_t currtimeMs = (int64_t)((esp_timer_get_time() / 1000) - delay_ms);
    timeElapsedInMS = currtimeMs - prev_time_ms;
    ESP_LOGE(DEMO_TAG, "Time elapsed since last packet received: %ims", (int) timeElapsedInMS);
    prev_time_ms = currtimeMs;
}

///Declare static functions
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 160,
    .scan_window            = 160,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};


static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        //the unit of the duration is second, 0 means scan permanently
        uint32_t duration = 0;
        esp_ble_gap_start_scanning(duration);
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        //scan start complete event to indicate scan start successfully or failed
        if ((err = param->scan_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(DEMO_TAG, "Scan start failed: %s", esp_err_to_name(err));
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            /* Search for BLE iBeacon Packet */
            if (esp_ble_is_ibeacon_packet(scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len)){
                if (firstPacketDetected == false)
                {
                    ESP_LOGI(DEMO_TAG, "First Packet Captured!");
                    prev_time_ms = (int64_t)(esp_timer_get_time() / 1000);
                    firstPacketDetected = true;
                }
                else
                {
                    logTimeSinceLastPacketReceived();
                    esp_ble_ibeacon_t *ibeacon_data = (esp_ble_ibeacon_t*)(scan_result->scan_rst.ble_adv);
                    ESP_LOGI(DEMO_TAG, "----------iBeacon Found----------");
                    ESP_LOG_BUFFER_HEX("IBEACON_DEMO: Device address:", scan_result->scan_rst.bda, ESP_BD_ADDR_LEN );
                    char tempBuf[16];
                    mempcpy(tempBuf, ibeacon_data->ibeacon_hello.msg, sizeof(tempBuf));
                    int64_t timeElapsedRounded = get_round_up_elapsed_time();
                    ESP_LOGI(DEMO_TAG, "Time Elapsed Rounded: %i ms", (int) timeElapsedRounded);
                    ESP_LOGI(DEMO_TAG, "Time Set By Sender  : %i ms", (int) ibeacon_data->pdu_arrival_ms.packetTimeArrivalMs);
                    if (((int) timeElapsedRounded) == ((int) ibeacon_data->pdu_arrival_ms.packetTimeArrivalMs))
                    {
                        decrypt_hello_msg(-1 * timeElapsedRounded, (uint8_t *) tempBuf, sizeof(tempBuf));
                        ESP_LOG_BUFFER_HEXDUMP("Decrypted MSG: ", tempBuf, sizeof(tempBuf), ESP_LOG_INFO);
                    }
                    //decrypt_hello_msg(-1 * timeElapsedRounded, (uint8_t *) tempBuf, sizeof(tempBuf));
                    //ESP_LOG_BUFFER_HEXDUMP("Decrypted MSG: ", tempBuf, sizeof(tempBuf), ESP_LOG_INFO);
                }
            }
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if ((err = param->scan_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(DEMO_TAG, "Scan stop failed: %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGI(DEMO_TAG, "Stop scan successfully");
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
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
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

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    ble_ibeacon_init();

    SpawnRecontructionKeyTask();
    
    //prev_time_ms = (int64_t)(esp_timer_get_time() / 1000);

    /* set scan parameters */
    //esp_ble_gap_set_scan_params(&ble_scan_params);
}
