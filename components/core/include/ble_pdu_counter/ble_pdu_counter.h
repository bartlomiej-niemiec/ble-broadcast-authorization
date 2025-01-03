#ifndef BLE_PDU_COUNTER_H
#define BLE_PDU_COUNTER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

esp_err_t init_pdu_counter();

uint32_t get_pdu_counter();

#endif