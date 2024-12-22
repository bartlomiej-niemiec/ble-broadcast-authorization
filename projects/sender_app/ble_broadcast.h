#ifndef BLE_BROADCAST_H
#define BLE_BROADCAST_H

#include "beacon_pdu/beacon_pdu_data.h"
#include "freertos/FreeRTOS.h"

BaseType_t queue_pdu_for_broadcast(beacon_pdu_data *pdu);

void start_up_broadcasting();

void stop_broadcasting();

bool is_broadcasting_running();

#endif