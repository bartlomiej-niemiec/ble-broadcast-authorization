#include "ble_security_payload_encryption.h"
#include "ble_broadcast_controller.h"
#include "esp_log.h"
#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdbool.h>


volatile uint32_t counter = 0;
volatile bool is_first_pdu = true;
static const char* SENDER_APP_LOG_GROUP = "SENDER APP";

#define ADV_INT_MIN_MS 100
#define ADV_INT_MAX_MS 110

#define N_CONST 0.625
#define MS_TO_N_CONVERTION(MS) ((uint16_t)((MS) / (N_CONST)))

static esp_ble_adv_params_t default_ble_adv_params = {
    .adv_int_min        = MS_TO_N_CONVERTION(ADV_INT_MIN_MS),
    .adv_int_max        = MS_TO_N_CONVERTION(ADV_INT_MAX_MS),
    .adv_type           = ADV_TYPE_NONCONN_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };


static uint32_t no_send_pdus = 0;

void ble_sender_main();
int build_new_payload();
bool encrypt_new_payload();
void data_set_success_cb()
{
    no_send_pdus++;
}

void app_main(void)
{
    init_payload_encryption();
    bool init_controller = init_broadcast_controller();
    if (init_controller == true)
    {
        register_broadcast_new_data_callback(data_set_success_cb);
        ble_sender_main();
    }
    else
    {
        ESP_LOGE(SENDER_APP_LOG_GROUP, "Failed to init broadcast controller");
    }
}

void ble_sender_main()
{
    const uint32_t TASK_DELAY_MS = 100;
    while (1)
    {
        bool result = encrypt_new_payload();
        if (!result)
        {
            vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS)); // Wait and continue the loop
            continue;
        }
            
        if (is_first_pdu == true)
        {
            start_broadcasting(&default_ble_adv_params);
            is_first_pdu = false;
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS)); // Wait before sending the next payload
    }
}

int build_new_payload(uint8_t *data, size_t size)
{
    memset(data, 0, sizeof(size));
    memcpy(data, &counter, sizeof(counter));
    //int result = snprintf(payload, sizeof(payload), "%s%i", PAYLOAD_HELLO, counter % 10000);
    return 0;
} 

bool encrypt_new_payload()
{
    counter++;
    uint8_t payload[MAX_PDU_PAYLOAD_SIZE] = {0};
    memcpy(payload, &counter, sizeof(counter));
    //ESP_LOG_BUFFER_HEX("Payload Set: ", payload, sizeof(payload));
    //ESP_LOGI(SENDER_APP_LOG_GROUP, "Payload uint32_t: %lu ", counter);
    // if (payload_build_size <= 0)
    // {
    //     ESP_LOGE(SENDER_APP_LOG_GROUP, "Failed to build payload");
    //     return false;
    // }
    beacon_pdu_data pdu = {0};
    fill_marker_in_pdu(&pdu);
    int encrypt_status = encrypt_payload(payload, sizeof(counter), &pdu);
    if (encrypt_status != 0)
    {
        ESP_LOGE(SENDER_APP_LOG_GROUP, "Failed to encrypt payload, error code: %d", encrypt_status);
        return false;
    }
            
    set_broadcasting_payload((uint8_t *)&pdu, get_beacon_pdu_data_len(&pdu));
    return true;
}