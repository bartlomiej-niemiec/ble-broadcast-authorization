#ifndef SEC_PDU_PROCESSING_H
#define SEC_PDU_PROCESSING_H

#include "esp_gap_ble_api.h"
#include "beacon_pdu_data.h"
#include <stdint.h>
#include <stddef.h>

typedef void (*payload_decrypted_notifier_cb)(uint8_t *data, size_t data_len, esp_bd_addr_t mac_address);

int start_up_sec_processing();

int process_sec_pdu(beacon_pdu_data* pdu, esp_bd_addr_t mac_address);

void register_payload_notifier_cb(payload_decrypted_notifier_cb cb);

#endif