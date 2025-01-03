#include "ble_broadcast_security_transmitter_engine.h"
#include "ble_pdu_counter.h"
#include "esp_log.h"
#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>

static const char PAYLOAD_HELLO[] = "HELLOWORD";

void app_main(void)
{
    bool status = init_payload_transmitter();
    if (status == true)
    {
        set_transmission_payload(PAYLOAD_HELLO, sizeof(PAYLOAD_HELLO));
        vTaskDelay(pdMS_TO_TICKS(30));
        startup_payload_transmitter_engine();
        init_pdu_counter();
    }
    else
    {
        ESP_LOGE("MAIN", "FAILED TO INITIALIZED PAYLOAD TRANSMITTER");
    }

    while (1)
    {
        set_transmission_payload(PAYLOAD_HELLO, sizeof(PAYLOAD_HELLO));
        vTaskDelay(pdMS_TO_TICKS(60));
    }
}
