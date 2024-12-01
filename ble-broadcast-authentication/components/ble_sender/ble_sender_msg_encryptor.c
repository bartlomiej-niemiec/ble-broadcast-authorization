#include "ble_sender_msg_encryptor.h"
#include <stdint.h>

uint16_t get_n_for_ms(uint16_t ms)
{
    return (uint16_t) (ms / 0.625);
}

uint16_t get_random_broadcast_interval_ms(uint16_t base_ms, uint16_t max_interval_ms)
{
    uint32_t randomNum = esp_random();
    uint32_t modulo = max_interval_ms / base_ms;
    return ((randomNum % modulo) + 1) * base_ms;  
}