#ifndef BLE_PDU_ENGINE_ENCRYPTOR_H
#define BLE_PDU_ENGINE_ENCRYPTOR_H

#include "stddef.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TRANSMITTER_NOT_INITIALIZED,
    TRANSMITTER_INITIALIZED_STOPPED,
    TRANSMITTER_WAIT_FOR_STARTUP,
    TRANSMITTER_KEY_GENERATION,
    TRANSMITTER_BROADCASTING_RUNNING,
    TRANSMITTER_PAYLOAD_SENDING
} TransmitterState;

bool init_payload_transmitter();

bool startup_payload_transmitter_engine();

bool stop_payload_transmitter_enginer();

int set_transmission_payload(uint8_t * payload, size_t payload_size);

TransmitterState get_transmitter_state();

#endif