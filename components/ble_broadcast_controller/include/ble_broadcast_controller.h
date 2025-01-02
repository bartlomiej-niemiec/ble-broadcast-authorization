#ifndef BLE_BROADCAST_CONTROLLER_H
#define BLE_BROADCAST_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>
#include "stddef.h"
#include "esp_gap_ble_api.h"

typedef enum {
    BROADCAST_CONTROLLER_BROADCASTING_NOT_RUNNING,
    BROADCAST_CONTROLLER_BROADCASTING_RUNNING
} BroadcastState;

typedef enum {
    SCANNER_CONTROLLER_SCANNING_NOT_ACTIVE,
    SCANNER_CONTROLLER_SCANNING_ACTIVE
} ScannerState;

typedef void (*broadcast_state_changed_callback)(BroadcastState currentBroadcastState);

typedef void (*broadcast_new_data_set_cb)();

typedef void (*scan_complete)(int64_t timestamp_us, uint8_t *data, size_t data_size, esp_bd_addr_t mac_address);

bool init_broadcast_controller();

void set_broadcasting_payload(uint8_t *payload, size_t payload_size);

void set_broadcast_time_interval(uint32_t time_interval_ms);

void start_broadcasting();

void stop_broadcasting();

void start_scanning(esp_ble_scan_params_t scan_params, uint32_t scan_duration_s);

void stop_scanning();

void register_broadcast_state_change_callback(broadcast_state_changed_callback cb);

void register_broadcast_new_data_callback(broadcast_new_data_set_cb cb);

void register_scan_complete_callback(scan_complete cb);

BroadcastState get_broadcast_state();

ScannerState get_scanner_state();

#endif