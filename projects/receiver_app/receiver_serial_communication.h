#ifndef RECEIVE_SERIAL_COMMUNICATION_H
#define RECEIVE_SERIAL_COMMUNICATION_H

#include <stdint.h>
#include <stddef.h>

void serial_data_received(uint8_t * data, size_t data_len);

#endif