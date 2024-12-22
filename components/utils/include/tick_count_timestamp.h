#ifndef TICK_COUNT_TIMESTAMP
#define TICK_COUNT_TIMESTAMP

#include "stdint.h"

void save_timestamp(uint64_t * tick_count_timestamp, uint8_t * rollover);

uint64_t get_timestamp(uint64_t * tick_count_timestamp, uint8_t * rollover);

void reset_timestamp(uint64_t * tick_count_timestamp, uint8_t * rollover);


#endif