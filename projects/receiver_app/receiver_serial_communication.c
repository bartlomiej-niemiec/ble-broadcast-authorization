#include "receiver_serial_communication.h"
#include "esp_log.h"

void serial_data_received(uint8_t * data, size_t data_len)
{
    ESP_LOG_BUFFER_CHAR("PC SERIAL COMM RECEIVED: ", (char*) data, data_len);
}