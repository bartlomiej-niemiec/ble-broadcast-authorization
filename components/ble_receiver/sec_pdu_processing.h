#ifndef SEC_PDU_PROCESSING_H
#define SEC_PDU_PROCESSING_H

#include "esp_gap_ble_api.h"
#include "sec_payload_decrypted_observer.h"
#include "beacon_pdu_data.h"
#include <stdint.h>
#include <stddef.h>

#define MAX_OBSERVERS 2

int start_up_sec_processing();

int enqueue_pdu_for_processing(beacon_pdu_data* pdu, esp_bd_addr_t mac_address);

void register_payload_observer_cb(payload_decrypted_observer_cb observer_cb);

#endif