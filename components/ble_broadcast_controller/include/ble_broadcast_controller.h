#ifndef BLE_BROADCAST_CONTROLLER_H
#define BLE_BROADCAST_CONTROLLER_H

#include <stdint.h>
#include "stddef.h"

typedef enum {
    BROADCAST_CONTROLLER_NOT_INITIALIZED,
    BROADCAST_CONTROLLER_INITIALIZED,
    BROADCAST_CONTROLLER_BROADCASTING_RUNNING,
    BROADCAST_CONTROLLER_BROADCASTING_FORCED_STOP,
} BroadcastState;

typedef void (*broadcast_state_changed_callback)(BroadcastState currentBroadcastState);

typedef void (*broadcast_new_data_set_cb)();

void init_broadcast_controller();

BroadcastState get_broadcast_state();

void set_broadcasting_payload(uint8_t *payload, size_t payload_size);

void set_broadcast_time_interval(uint32_t time_interval_ms);

void start_broadcasting();

void stop_broadcasting();

void register_broadcast_state_change_callback(broadcast_state_changed_callback cb);

void register_broadcast_new_data_callback(broadcast_new_data_set_cb cb);

#endif