#include "tick_count_timestamp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void save_timestamp(uint64_t * tick_count_timestamp, uint8_t * rollover)
{
    if (tick_count_timestamp != NULL && rollover != NULL)
    {
        uint32_t tickCount = xTaskGetTickCount();
        if (tickCount < *tick_count_timestamp)
        {
           (*rollover)++;
        }
        *tick_count_timestamp = tickCount;
    }
}

uint64_t get_timestamp(uint64_t * tick_count_timestamp, uint8_t * rollover)
{
    if (tick_count_timestamp == NULL || rollover == NULL)
    {
        return 0;
    }
    return ((uint64_t)(*rollover) << 32) | (*tick_count_timestamp);
}

void reset_timestamp(uint64_t * tick_count_timestamp, uint8_t * rollover)
{
    if (tick_count_timestamp != NULL && rollover != NULL)
    {
        rollover = 0;
        tick_count_timestamp = 0;
    }
}