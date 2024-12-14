#include "ble_sender.h" 
#include "ble_broadcast.h"
#include "ble_msg_encryptor.h"
#include "esp_log.h"
#include "stdio.h"
#include "freertos/task.h"
#include <stdbool.h>


char payload[MAX_PDU_PAYLOAD_SIZE] = {0};
volatile uint16_t counter = 0;
static const char* SENDER_APP_LOG_GROUP = "SENDER APP";
static const char PAYLOAD_HELLO[] = "HELLO WORD";


void ble_sender_main();
int build_new_payload();

void ble_start_sender(void)
{
    init_payload_encryption();
    start_up_broadcasting();
    ble_sender_main();

}

void ble_sender_main()
{
    const uint32_t TASK_DELAY_MS = 100;
    beacon_pdu_data pdu;
    fill_marker_in_pdu(&pdu);

    while (1)
    {
        counter++;
        if (is_broadcasting_running() == true)
        {
            // Build new payload
            int payload_build_size = build_new_payload();
            if (payload_build_size <= 0)
            {
                ESP_LOGE(SENDER_APP_LOG_GROUP, "Failed to build payload");
                vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS)); // Wait and continue the loop
                continue;  // Skip the rest of the loop if payload building fails
            }

            // Encrypt the payload
            int encrypt_status = encrypt_payload((uint8_t *) &payload[0], payload_build_size, &pdu);
            if (encrypt_status != 0)
            {
                ESP_LOGE(SENDER_APP_LOG_GROUP, "Failed to encrypt payload, error code: %d", encrypt_status);
                vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS)); // Wait and continue the loop
                continue;
            }

            // Log the encrypted PDU details
            // ESP_LOGI(SENDER_APP_LOG_GROUP, "Encrypted payload: %s", payload);

            // Add the PDU to the queue for broadcasting
            if (queue_pdu_for_broadcast(&pdu) != pdTRUE)
            {
                ESP_LOGE(SENDER_APP_LOG_GROUP, "Failed to add PDU to broadcast queue");
            }
            else
            {
                //ESP_LOGI(SENDER_APP_LOG_GROUP, "Successfully added PDU to queue");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS)); // Wait before sending the next payload
    }
}

int build_new_payload()
{
    int result = snprintf(payload, sizeof(payload), "%s %i", PAYLOAD_HELLO, counter % 99);
    return result;
} 