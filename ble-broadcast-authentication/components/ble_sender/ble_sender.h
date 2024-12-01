#ifndef BLE_SENDER_H
#define BLE_SENDER_H

#include "beacon_pdu_data.h"

typedef struct {
    beacon_pdu_data pdu;
    uint16_t time_interval_ms;
} beacon_pdu_with_time_interval;

void ble_start_sender();

void queue_pdu_to_send(beacon_pdu_with_time_interval *pdu);

#endif