#ifndef PC_SERIAL_COMMUNICATION_H
#define PC_SERIAL_COMMUNICATION_H

#include "stdint.h"
#include "stddef.h"
#include "esp_err.h"

typedef void (*serial_data_received_cb)(uint8_t * data, size_t data_len);

void register_serial_data_received_cb(serial_data_received_cb cb);

void send_test_string_to_pc();

esp_err_t start_pc_serial_communication();

#endif