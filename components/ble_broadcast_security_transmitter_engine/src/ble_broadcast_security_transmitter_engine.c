#include <stdint.h>
#include "stddef.h"
#include <string.h>

#include "ble_broadcast_security_transmitter_engine.h"
#include "ble_broadcast_security_encryptor.h"
#include "ble_broadcast_controller.h"
#include "beacon_pdu_data.h"
#include "beacon_pdu_key_reconstruction.h"
#include "beacon_pdu_helpers.h"
#include "crypto.h"
#include "ble_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"


#define TRANSMITTER_QUEUE_SIZE 10
#define TRANSMITTER_SEMAPHORE_TIMEOUT_MS 30
#define TRANSMITTER_QUEUE_TIMEOUT_MS 30
#define TRANSMITTER_TASK_DELAY_MS 20

#define TRANSMITTER_TASK_STACK_SIZE 4096
#define TRANSMITTER_TASK_PRIORITY 5
#define TRANSMITTER_CORE 1

static const char * TRANSMITTER_TASK_NAME = "TRANSMITTER";
static const char * TRANSMITTER_TASK_LOG =  "TRANSMITTER_LOG";

typedef struct {
    uint8_t* data;
    size_t size;
} queue_payload_pdu;

typedef struct {
    esp_timer_handle_t timer;
    esp_timer_create_args_t timer_args;
    const char timer_name[30];
} key_fragment_schedule_timer;

static key_encryption_parameters transmitter_key_encryption_pars;

typedef struct {
    bool key_fragment_ready_to_send;
    beacon_key_pdu bpd;
} key_fragment_structure;

static key_fragment_structure key_fragment_st = {
    .key_fragment_ready_to_send = false
};

typedef struct {
    QueueHandle_t xQueuePayloadPdu;
    TaskHandle_t xTaskHandle;
    key_fragment_schedule_timer key_fragment_schedule_st;
    SemaphoreHandle_t xBroadcastPayloadMutex;
    TransmitterState state;
    key_128b key;
    key_splitted key_fragments;    
    uint32_t session_id; 
    SemaphoreHandle_t xMutex;
    queue_payload_pdu last_payload_pdu; //use it when there is no new payload and no stop command
    queue_payload_pdu queue_add_helper;
} sec_pdu_transmitter_control;

static sec_pdu_transmitter_control transmitter_control_st = 
{
    .state = TRANSMITTER_NOT_INITIALIZED,
    .key_fragment_schedule_st.timer_name = "SCHEDULE TIMER"
};



void transmitter_main_loop();
void key_generation_state();
bool encrypt_payload_data(uint8_t *payload, size_t size, beacon_pdu_data *pdu);

uint32_t get_random_time_interval()
{
    uint32_t scale = (uint32_t) (MAX_TIME_INTERVAL_MS / TIME_INTERVAL_RESOLUTION_MS);
    uint32_t time_interval_ms = ((esp_random() % scale) + 1) * TIME_INTERVAL_RESOLUTION_MS;
    return time_interval_ms;
}

void key_fragment_ready(beacon_key_pdu * bkpdu)
{
    ESP_LOGI(TRANSMITTER_TASK_LOG, "Key fragment has been encrypted!");
    memcpy(&key_fragment_st.bpd, bkpdu, sizeof(beacon_key_pdu));
    key_fragment_st.key_fragment_ready_to_send = true;
}

void key_fragment_send_timer(void *args)
{
    if (key_fragment_st.key_fragment_ready_to_send == true)
    {
        if(xSemaphoreTake(transmitter_control_st.xBroadcastPayloadMutex, 0) == pdTRUE)
        {
            ESP_LOGI(TRANSMITTER_TASK_LOG, "Key fragment PDU set!");
            set_broadcasting_payload(&key_fragment_st.bpd, sizeof(key_fragment_st.bpd));
            uint32_t time_interval = get_random_time_interval();
            ESP_LOGI(TRANSMITTER_TASK_LOG, "Request for next key fragment after %lu", time_interval);
            transmitter_key_encryption_pars.last_time_interval_ms += transmitter_key_encryption_pars.time_interval_ms;
            transmitter_key_encryption_pars.time_interval_ms = time_interval;
            transmitter_key_encryption_pars.key_fragment_index++;
            if (transmitter_key_encryption_pars.key_fragment_index == 4)
            {
                transmitter_key_encryption_pars.key_fragment_index = 0;
            }
            key_fragment_st.key_fragment_ready_to_send = false;
            request_key_fragment_pdu_encryption(&transmitter_key_encryption_pars);
            esp_timer_start_once(transmitter_control_st.key_fragment_schedule_st.timer, time_interval * 1000);
            vTaskDelay(pdMS_TO_TICKS(TIME_INTERVAL_RESOLUTION_MS));
            set_broadcasting_payload(&transmitter_control_st.last_payload_pdu.data, transmitter_control_st.last_payload_pdu.size);
            xSemaphoreGive(transmitter_control_st.xBroadcastPayloadMutex);
        }
        
    }
}

bool is_pdu_in_queue()
{
    return uxQueueMessagesWaiting(transmitter_control_st.xQueuePayloadPdu) > 0;
}


TransmitterState get_transmitter_state()
{
    TransmitterState currState = TRANSMITTER_NOT_INITIALIZED;
    if (transmitter_control_st.xMutex != NULL)
    {
        if (xSemaphoreTake(transmitter_control_st.xMutex, pdMS_TO_TICKS(TRANSMITTER_SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
        {
            currState = transmitter_control_st.state;
            xSemaphoreGive(transmitter_control_st.xMutex);
        }
    }
    return currState;
}

bool set_transmitter_state(TransmitterState desired_state)
{
    bool status = false;
    if (transmitter_control_st.xMutex != NULL)
    {
        if (xSemaphoreTake(transmitter_control_st.xMutex, pdMS_TO_TICKS(TRANSMITTER_SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
        {
            ESP_LOGI(TRANSMITTER_TASK_LOG, "State transition: %d -> %d", transmitter_control_st.state, desired_state);
            transmitter_control_st.state = desired_state;
            status = true;
            xSemaphoreGive(transmitter_control_st.xMutex);
        }
    }
    return status;
}

bool init_payload_transmitter()
{
    if (transmitter_control_st.state == TRANSMITTER_NOT_INITIALIZED)
    {
        transmitter_control_st.xQueuePayloadPdu = xQueueCreate(TRANSMITTER_QUEUE_SIZE, sizeof(queue_payload_pdu));
        if (transmitter_control_st.xQueuePayloadPdu == NULL)
        {
            ESP_LOGE(TRANSMITTER_TASK_LOG, "Failed to create Queue");
            return false;
        }

        transmitter_control_st.xMutex = xSemaphoreCreateMutex();
        if (transmitter_control_st.xMutex == NULL)
        {
            ESP_LOGE(TRANSMITTER_TASK_LOG, "Failed to create Mutex");
            vQueueDelete(transmitter_control_st.xQueuePayloadPdu);
            return false;
        }

        transmitter_control_st.xBroadcastPayloadMutex = xSemaphoreCreateMutex();
        if (transmitter_control_st.xMutex == NULL)
        {
            ESP_LOGE(TRANSMITTER_TASK_LOG, "Failed to create Mutex");
            vQueueDelete(transmitter_control_st.xQueuePayloadPdu);
            return false;
        }

        transmitter_control_st.key_fragment_schedule_st.timer_args.callback = key_fragment_send_timer;
        transmitter_control_st.key_fragment_schedule_st.timer_args.arg = NULL;
        transmitter_control_st.key_fragment_schedule_st.timer_args.name = transmitter_control_st.key_fragment_schedule_st.timer_name;

        if (transmitter_control_st.key_fragment_schedule_st.timer == NULL)
        {
            esp_timer_create(&transmitter_control_st.key_fragment_schedule_st.timer_args, &transmitter_control_st.key_fragment_schedule_st.timer);
            if (transmitter_control_st.key_fragment_schedule_st.timer == NULL)
            {
                ESP_LOGE(TRANSMITTER_TASK_LOG, "Failed to create timer");
                return false;
            }
        }

        ble_init();
        init_broadcast_controller();
        const uint32_t WAIT_MS = 50;
        vTaskDelay(pdMS_TO_TICKS(WAIT_MS));
        if (get_broadcast_state() == BROADCAST_CONTROLLER_NOT_INITIALIZED)
        {
            ESP_LOGE(TRANSMITTER_TASK_LOG, "Failed to initialized broadcast controller");
            vQueueDelete(transmitter_control_st.xQueuePayloadPdu);
            return false;
        }

        bool result = init_and_start_up_encryptor_task();
        register_fragment_encrypted_callback(key_fragment_ready);
        if (result == false)
        {
            ESP_LOGE(TRANSMITTER_TASK_LOG, "Failed to create encryptor task");
            vQueueDelete(transmitter_control_st.xQueuePayloadPdu);
            return false;
        }

        transmitter_control_st.queue_add_helper.data = (uint8_t *) calloc(MAX_PDU_PAYLOAD_SIZE, sizeof(uint8_t));
        if (transmitter_control_st.queue_add_helper.data == NULL)
        {
            ESP_LOGE(TRANSMITTER_TASK_LOG, "Failed to allocate mem for queue helper");
            vQueueDelete(transmitter_control_st.xQueuePayloadPdu);
            return false;
        } 

        transmitter_control_st.last_payload_pdu.data = (uint8_t *) calloc(MAX_PDU_PAYLOAD_SIZE, sizeof(uint8_t));
        if (transmitter_control_st.last_payload_pdu.data == NULL)
        {
            ESP_LOGE(TRANSMITTER_TASK_LOG, "Failed to allocate mem for last_payload_pdu");
            vQueueDelete(transmitter_control_st.xQueuePayloadPdu);
            return false;
        } 

        set_transmitter_state(TRANSMITTER_INITIALIZED_STOPPED);
    }


    if (transmitter_control_st.state == TRANSMITTER_INITIALIZED_STOPPED)
    {
        BaseType_t  taskCreateResult = xTaskCreatePinnedToCore(
                    (TaskFunction_t) transmitter_main_loop,
                    TRANSMITTER_TASK_NAME, 
                    (uint32_t) TRANSMITTER_TASK_STACK_SIZE,
                    NULL,
                    (UBaseType_t) TRANSMITTER_TASK_PRIORITY,
                    (TaskHandle_t * )&(transmitter_control_st.xTaskHandle),
                    TRANSMITTER_CORE
                    );
                
        if (taskCreateResult != pdPASS)
        {
            transmitter_control_st.xTaskHandle = NULL;
            ESP_LOGE(TRANSMITTER_TASK_LOG, "Task was not created successfully! :(");
            return false;
        }
        else
        {
            ESP_LOGI(TRANSMITTER_TASK_LOG, "Task was created successfully! :)");
            set_transmitter_state(TRANSMITTER_WAIT_FOR_STARTUP);
        }
    }

    return true;
}

void transmitter_main_loop()
{

    uint32_t TASK_DELAY_TICKS = pdMS_TO_TICKS(TRANSMITTER_TASK_DELAY_MS);
    beacon_pdu_data payload_pdu;
    beacon_key_pdu key_pdu;
    queue_payload_pdu queue_pdu;
    bool firstPDU = true;
    while (1)
    {
        switch (transmitter_control_st.state)
        {
            case TRANSMITTER_WAIT_FOR_STARTUP:
            {
                // DO NOTHING WAIT FOR START COMMAND
            }
            break;

            case TRANSMITTER_KEY_GENERATION:
            {
                key_generation_state();
                set_transmitter_state(TRANSMITTER_BROADCASTING_RUNNING);
            }
            break;

            case TRANSMITTER_BROADCASTING_RUNNING:
            {
                if (is_pdu_in_queue())
                {
                    if (xQueueReceive(transmitter_control_st.xQueuePayloadPdu, &queue_pdu, 0) == pdTRUE)
                    {
                        bool status = encrypt_payload_data(queue_pdu.data, queue_pdu.size, &payload_pdu);
                        if (status)
                        {
                            if (xSemaphoreTake(transmitter_control_st.xBroadcastPayloadMutex, 0) == pdTRUE)
                            {
                                set_broadcasting_payload(&payload_pdu, sizeof(payload_pdu));
                                if (firstPDU == true)
                                {
                                    start_broadcasting();
                                    int64_t current_time = esp_timer_get_time() / 1000;
                                    uint32_t time_interval = get_random_time_interval();
                                    transmitter_key_encryption_pars.last_time_interval_ms = current_time;
                                    transmitter_key_encryption_pars.time_interval_ms = time_interval;
                                    request_key_fragment_pdu_encryption(&transmitter_key_encryption_pars);
                                    esp_timer_start_once(transmitter_control_st.key_fragment_schedule_st.timer, time_interval * 1000);
                                    firstPDU = false;
                                }

                                memcpy(transmitter_control_st.last_payload_pdu.data, queue_pdu.data, queue_pdu.size);
                                transmitter_control_st.last_payload_pdu.size = queue_pdu.size;

                                xSemaphoreGive(transmitter_control_st.xBroadcastPayloadMutex);
                            }
                        }
                    }
                    else
                    {
                        ESP_LOGE(TRANSMITTER_TASK_LOG, "Failed to acquire  xBroadcastPayloadMutex");
                    }
                }

            }
            break;

            case TRANSMITTER_INITIALIZED_STOPPED:
            {
                
            }
            break;

            default:
                break;
        }

        vTaskDelay(TASK_DELAY_TICKS);

    }
}

void key_generation_state()
{
    generate_128b_key(&transmitter_control_st.key);
    split_128b_key_to_fragment(&transmitter_control_st.key, &transmitter_control_st.key_fragments);
    transmitter_control_st.session_id = esp_random();

    memcpy(&transmitter_key_encryption_pars.key_fragments, &transmitter_control_st.key_fragments, sizeof(transmitter_control_st.key_fragments));
    transmitter_key_encryption_pars.session_id = transmitter_control_st.session_id;
    transmitter_key_encryption_pars.key_fragment_index = 0;
}

bool startup_payload_transmitter_engine()
{
    TransmitterState state = get_transmitter_state();
    if (state != TRANSMITTER_NOT_INITIALIZED)
    {
        if (state == TRANSMITTER_WAIT_FOR_STARTUP && is_pdu_in_queue())
        {
            set_transmitter_state(TRANSMITTER_KEY_GENERATION);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        state = get_transmitter_state();
    }
    return  state > TRANSMITTER_WAIT_FOR_STARTUP ? true : false;
}

bool stop_payload_transmitter_engine()
{
    TransmitterState state = get_transmitter_state();
    if (state != TRANSMITTER_NOT_INITIALIZED && state != TRANSMITTER_INITIALIZED_STOPPED && state != TRANSMITTER_WAIT_FOR_STARTUP)
    {
        set_transmitter_state(TRANSMITTER_INITIALIZED_STOPPED);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    state = get_transmitter_state();
    return  state == TRANSMITTER_INITIALIZED_STOPPED ? true : false;
}

int set_transmission_payload(uint8_t * payload, size_t payload_size)
{
    if (payload_size > MAX_PDU_PAYLOAD_SIZE)
        return -1;


    int status = -2;

    if (transmitter_control_st.state != TRANSMITTER_NOT_INITIALIZED && transmitter_control_st.state != TRANSMITTER_INITIALIZED_STOPPED)
    {
        memcpy(transmitter_control_st.queue_add_helper.data, payload, payload_size);
        transmitter_control_st.queue_add_helper.size = payload_size;
        status = xQueueSend(transmitter_control_st.xQueuePayloadPdu, &transmitter_control_st.queue_add_helper, TRANSMITTER_QUEUE_TIMEOUT_MS);
    }

    return status;
}

bool encrypt_payload_data(uint8_t *payload, size_t size, beacon_pdu_data *pdu) {
    if (MAX_PDU_PAYLOAD_SIZE < size)
        return false;

    uint8_t encrypt_payload_arr[MAX_PDU_PAYLOAD_SIZE] = {0};
    uint8_t nonce[NONCE_SIZE];

    // Generate random nonce
    fill_random_bytes(nonce, sizeof(nonce));

    aes_ctr_encrypt_payload(payload,
            size,
            transmitter_control_st.key.key,
            nonce,
            encrypt_payload_arr
            );

    build_beacon_pdu_data(encrypt_payload_arr,
            size,
            (uint8_t *)&transmitter_control_st.session_id,
            sizeof(transmitter_control_st.session_id),
            nonce,
            &pdu
    );

    return true;
}