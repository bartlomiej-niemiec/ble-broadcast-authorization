#ifndef SEC_PAYLOAD_DECRYPTED_OBSERVER_H
#define SEC_PAYLOAD_DECRYPTED_OBSERVER_H

#include <stdint.h>
#include "esp_gap_ble_api.h"

typedef void (*payload_decrypted_observer_cb)(uint8_t *data, size_t data_len, esp_bd_addr_t mac_address);

#endif