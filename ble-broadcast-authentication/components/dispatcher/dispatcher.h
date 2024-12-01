#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <stdint.h>
#include <stddef.h>

#define DISPATCHER_TASK_SIZE 4096
#define DISPATCHER_TASK_PRIORITY 8
#define DISPATCHER_TASK_CORE 1

#define MAX_BLE_BROADCAST_SIZE_BYTES 31

typedef struct{
    uint8_t data[MAX_BLE_BROADCAST_SIZE_BYTES];
    int64_t timeinterval;
} ble_broadcast_pdu;

void SpawnDispatcherTask(void);

void AddPduToDispatcher(ble_broadcast_pdu* pdu);

void create_ble_broadcast_pdu_for_dispatcher(ble_broadcast_pdu* pdu, uint8_t *data, size_t size, int64_t timeinterval);

#endif