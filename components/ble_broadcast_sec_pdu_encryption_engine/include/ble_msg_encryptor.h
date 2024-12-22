#ifndef BLE_SENDER_MSG_ENCRYPTOR_H
#define BLE_SENDER_MSG_ENCRYPTO_H

#include "beacon_pdu_data.h"
#include "stddef.h"

bool init_payload_encryption();

int encrypt_payload(uint8_t * payload, size_t payload_size, beacon_pdu_data * encrypted_pdu);

#endif