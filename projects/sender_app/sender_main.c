#include "ble_security_payload_encryption.h"
#include "ble_broadcast_controller.h"
#include "esp_log.h"
#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdbool.h>

typedef enum {
    SENDER_WAIT_FOR_START_CMD,
    SENDER_TEST_START_PDU,
    SENDER_BROADCAST_PDU,
    SENDER_TEST_END_PDU
} sender_state_machine;

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

void sender_wair_for_start_cmd();
void sender_test_start_pdu();
void sender_broadcast_pdu();
void sender_test_end_pdu();
int build_new_payload(uint8_t *data, size_t size);
bool encrypt_new_payload();

void ble_sender_main()
{
    int state_machine_state = SENDER_TEST_START_PDU;
    while (1)
    {

        switch (state_machine_state)
        {
            case SENDER_WAIT_FOR_START_CMD:
            {
                sender_wair_for_start_cmd();
            }
            break;

            case SENDER_TEST_START_PDU:
            {
                sender_test_start_pdu();
            }
            break;

            case SENDER_BROADCAST_PDU:
            {
                sender_broadcast_pdu();
            }
            break;

            case SENDER_TEST_END_PDU:
            {
                sender_test_end_pdu();
            }
            break;
        }
    }
}

void sender_wair_for_start_cmd();

void sender_test_start_pdu();

void sender_broadcast_pdu()
{
    static const uint32_t TASK_DELAY_MS = 100;
    bool result = encrypt_new_payload();
    if (!result)
    {
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS)); // Wait and continue the loop
        return;
    }
            
    if (is_first_pdu == true)
    {
        start_broadcasting(&default_ble_adv_params);
        is_first_pdu = false;
    }

    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS)); // Wait before sending the next payload
}

void sender_test_end_pdu();



int build_new_payload(uint8_t *data, size_t size)
{
    memset(data, 0, sizeof(size));
    memcpy(data, &counter, sizeof(counter));
    return 0;
} 

bool encrypt_new_payload()
{
    counter++;
    uint8_t payload[MAX_PDU_PAYLOAD_SIZE] = {0};
    memcpy(payload, &counter, sizeof(counter));
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