#include "ble_security_payload_encryption.h"
#include "ble_broadcast_controller.h"
#include "esp_log.h"
#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "pc_serial_communication.h"
#include "beacon_test_pdu.h"

#include "test.h"

#include <string.h>
#include <stdbool.h>

#define MAX_SERIAL_MSG_SIZE 30

#define TASK_DELAY_MS 50
#define TASK_DELAY_SYSTICK pdMS_TO_TICKS(TASK_DELAY_MS)

typedef enum {
    SENDER_WAIT_FOR_START_CMD,
    SENDER_TEST_START_PDU,
    SENDER_BROADCAST_PDU,
    SENDER_TEST_END_PDU
} sender_state_machine;

volatile uint32_t counter = 0;
volatile bool is_first_pdu = true;
static const char* SENDER_APP_LOG_GROUP = "SENDER APP";
static char PC_SERIAL_BUFFER[MAX_SERIAL_MSG_SIZE] = {0};
static const char* START_CMD_SERIAL = "START_TEST";
static SemaphoreHandle_t xStartCmdReceived;

#define ADV_INT_MIN_MS 100
#define ADV_INT_MAX_MS 110

#define N_CONST 0.625
#define MS_TO_N_CONVERTION(MS) ((uint16_t)((MS) / (N_CONST)))

#define MAX_BLOCK_TIME_SEMAPHORE_MS 100
#define MAX_BLOCK_TIME_SEMAPHORE_TICKS pdMS_TO_TICKS(MAX_BLOCK_TIME_SEMAPHORE_MS)

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

void serial_data_received(uint8_t * data, size_t data_len)
{
    memcpy(PC_SERIAL_BUFFER, (char *) data, data_len);
    PC_SERIAL_BUFFER[data_len] = '\0';
    ESP_LOGI(SENDER_APP_LOG_GROUP, "PC SERIAL MSG: %s", PC_SERIAL_BUFFER);
    if (strstr(PC_SERIAL_BUFFER, START_CMD_SERIAL) != NULL)
    {
        xSemaphoreGive(xStartCmdReceived);
    }
}

void app_main(void)
{
    init_payload_encryption();
    bool init_controller = init_broadcast_controller();

    xStartCmdReceived = xSemaphoreCreateBinary();
    if (xStartCmdReceived == NULL)
        return;
    xSemaphoreTake(xStartCmdReceived, MAX_BLOCK_TIME_SEMAPHORE_TICKS);

    if (init_controller == true)
    {
        register_broadcast_new_data_callback(data_set_success_cb);

        if (start_pc_serial_communication() != ESP_OK)
        {
            ESP_LOGE(SENDER_APP_LOG_GROUP, "Failed initializtion of pc communication");
            return;
        }
        register_serial_data_received_cb(serial_data_received);

        ble_sender_main();
    }
    else
    {
        ESP_LOGE(SENDER_APP_LOG_GROUP, "Failed to init broadcast controller");
    }
}

void sender_wair_for_start_cmd(int * state);
void sender_test_start_pdu(int * state);
void sender_broadcast_pdu(int * state);
void sender_test_end_pdu(int * state);
int build_new_payload(uint8_t *data, size_t size);
bool encrypt_new_payload();

void ble_sender_main()
{
    int state_machine_state = SENDER_WAIT_FOR_START_CMD;
    while (1)
    {
        switch (state_machine_state)
        {
            case SENDER_WAIT_FOR_START_CMD:
            {
                sender_wair_for_start_cmd(&state_machine_state);
            }
            break;

            case SENDER_TEST_START_PDU:
            {
                sender_test_start_pdu(&state_machine_state);
            }
            break;

            case SENDER_BROADCAST_PDU:
            {
                sender_broadcast_pdu(&state_machine_state);
            }
            break;

            case SENDER_TEST_END_PDU:
            {
                sender_test_end_pdu(&state_machine_state);
            }
            break;
        }
    }
}

void sender_wair_for_start_cmd(int *state)
{
    if (xSemaphoreTake(xStartCmdReceived, MAX_BLOCK_TIME_SEMAPHORE_TICKS) == pdPASS)
    {
        ESP_LOGI(SENDER_APP_LOG_GROUP, "Received Start Command From Pc");
        *state = SENDER_TEST_START_PDU;
    }
    vTaskDelay(TASK_DELAY_SYSTICK);
}

void sender_test_start_pdu(int *state)
{
    ESP_LOGI(SENDER_APP_LOG_GROUP, "Sending Start Broadcast PDUs");
    beacon_test_pdu pdu;
    if (build_test_start_pdu(&pdu) == ESP_OK)
    {
        set_broadcasting_payload((uint8_t *)&pdu, sizeof(beacon_test_pdu));
    }
    start_broadcasting(&default_ble_adv_params);
    vTaskDelay(ADV_INT_MAX_MS * 10);
    init_test();
    start_test_measurment();
    ESP_LOGI(SENDER_APP_LOG_GROUP, "Changing state to broadcast pdus");
    *state = SENDER_BROADCAST_PDU;
}

void sender_broadcast_pdu(int *state)
{
    static const uint32_t TEMP_TASK_DELAY_MS = 100;
    bool result = encrypt_new_payload();
    if (!result)
    {
        vTaskDelay(pdMS_TO_TICKS(TEMP_TASK_DELAY_MS)); // Wait and continue the loop
        return;
    }
        
    vTaskDelay(pdMS_TO_TICKS(TEMP_TASK_DELAY_MS)); // Wait before sending the next payload
}

void sender_test_end_pdu(int *state)
{
    beacon_test_pdu pdu;
    if (build_test_end_pdu(&pdu) == ESP_OK)
    {
        set_broadcasting_payload((uint8_t *)&pdu, sizeof(beacon_test_pdu));
    }
    vTaskDelay(ADV_INT_MAX_MS * 10);
    stop_broadcasting();
}

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