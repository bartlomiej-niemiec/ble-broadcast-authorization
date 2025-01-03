#include <stdint.h>
#include "stddef.h"
#include <string.h>

#include "ble_broadcast_security_transmitter_engine.h"
#include "ble_broadcast_controller.h"
#include "beacon_pdu_data.h"
#include "beacon_pdu_key_reconstruction.h"
#include "beacon_pdu_helpers.h"
#include "crypto.h"
#include "ble_common.h"

#include <stdatomic.h>

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

typedef struct {
    uint8_t key_fragment;
    uint32_t last_pdu_timestamp;
    beacon_key_data key_data;
    beacon_key_pdu encrypted_key_pdu;
} key_encryption_parameters;

typedef struct {
    QueueHandle_t xQueuePayloadPdu;
    TaskHandle_t xTaskHandle;
    key_fragment_schedule_timer key_fragment_schedule_st;
    SemaphoreHandle_t xBroadcastPayloadMutex;
    atomic_int   transmitterState;
    key_encryption_parameters transmitter_key_encryption_pars;
    key_128b key;
    key_splitted key_fragments;    
    uint32_t session_id; 
    queue_payload_pdu last_payload_pdu; //use it when there is no new payload and no stop command
    queue_payload_pdu queue_add_helper;
} sec_pdu_transmitter_control;

static sec_pdu_transmitter_control transmitter_control_st = 
{
    .key_fragment_schedule_st.timer_name = "SCHEDULE TIMER",
    .transmitterState = TRANSMITTER_NOT_INITIALIZED
};

void transmitter_main_loop();
void key_generation_state();
bool encrypt_payload_data(uint8_t *payload, size_t size, beacon_pdu_data *pdu);
void start_up_key_fragment_sending();
void increment_next_key_fragment_index();
uint32_t get_abs_timestamp_ms();
void save_last_pdu_timestamp();
void encrypt_nexy_key_fragment(uint32_t time_interval_ms);

uint32_t get_random_time_interval()
{
    uint32_t scale = (uint32_t) (MAX_TIME_INTERVAL_MS / TIME_INTERVAL_RESOLUTION_MS);
    uint32_t time_interval_ms = ((esp_random() % scale) + 1) * TIME_INTERVAL_RESOLUTION_MS;
    return time_interval_ms;
}

void key_fragment_send_timer(void *args)
{
        if(xSemaphoreTake(transmitter_control_st.xBroadcastPayloadMutex, 0) == pdTRUE)
        {
            const int64_t DELAY_TIME = TIME_INTERVAL_RESOLUTION_MS * 2;
            const int64_t CURR_TIMESTAMP = get_abs_timestamp_ms();
            ESP_LOGI(TRANSMITTER_TASK_LOG, "Key fragment PDU set!");
            set_broadcasting_payload(&transmitter_control_st.transmitter_key_encryption_pars.encrypted_key_pdu, sizeof(transmitter_control_st.transmitter_key_encryption_pars.encrypted_key_pdu));
            uint32_t time_interval_ms = get_random_time_interval();
            encrypt_nexy_key_fragment(time_interval_ms);
            ESP_LOGI(TRANSMITTER_TASK_LOG, "Request for next key fragment after %lu", time_interval_ms);
            esp_timer_start_once(transmitter_control_st.key_fragment_schedule_st.timer, time_interval_ms * 1000);
            vTaskDelay(pdMS_TO_TICKS(DELAY_TIME - (get_abs_timestamp_ms() - CURR_TIMESTAMP)));
            set_broadcasting_payload(&transmitter_control_st.last_payload_pdu.data, transmitter_control_st.last_payload_pdu.size);
            xSemaphoreGive(transmitter_control_st.xBroadcastPayloadMutex);
        }
}

bool is_pdu_in_queue()
{
    return uxQueueMessagesWaiting(transmitter_control_st.xQueuePayloadPdu) > 0;
}


TransmitterState get_transmitter_state()
{
    return atomic_load(&transmitter_control_st.transmitterState);
}

bool set_transmitter_state(TransmitterState desired_state)
{
    ESP_LOGI(TRANSMITTER_TASK_LOG, "State transition: %d -> %d", get_transmitter_state(), desired_state);
    atomic_store(&transmitter_control_st.transmitterState, desired_state);
    return true;
}

bool init_payload_transmitter()
{
    if (get_transmitter_state() == TRANSMITTER_NOT_INITIALIZED)
    {
        transmitter_control_st.xQueuePayloadPdu = xQueueCreate(TRANSMITTER_QUEUE_SIZE, sizeof(queue_payload_pdu));
        if (transmitter_control_st.xQueuePayloadPdu == NULL)
        {
            ESP_LOGE(TRANSMITTER_TASK_LOG, "Failed to create Queue");
            return false;
        }

        transmitter_control_st.xBroadcastPayloadMutex = xSemaphoreCreateMutex();
        if (transmitter_control_st.xBroadcastPayloadMutex == NULL)
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
        bool status = init_broadcast_controller();
        if (status == false)
        {
            ESP_LOGE(TRANSMITTER_TASK_LOG, "Failed to initialized broadcast controller");
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


    if (get_transmitter_state() == TRANSMITTER_INITIALIZED_STOPPED)
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
    queue_payload_pdu queue_pdu;
    bool firstPDU = true;
    while (1)
    {
        switch (get_transmitter_state())
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
                                    start_up_key_fragment_sending();
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

    transmitter_control_st.transmitter_key_encryption_pars.key_data.session_data = transmitter_control_st.session_id;
    transmitter_control_st.transmitter_key_encryption_pars.key_fragment = 0;
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

    TransmitterState state = get_transmitter_state();
    if ( state != TRANSMITTER_NOT_INITIALIZED && state != TRANSMITTER_INITIALIZED_STOPPED)
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


    encrypt_payload(
        transmitter_control_st.key.key,
        payload,
        size,
        (uint8_t *)&transmitter_control_st.session_id,
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

uint32_t get_abs_timestamp_ms()
{
    return (uint32_t) (esp_timer_get_time() / 1000);
}

void increment_next_key_fragment_index()
{
    transmitter_control_st.transmitter_key_encryption_pars.key_fragment++;
    if (transmitter_control_st.transmitter_key_encryption_pars.key_fragment == 4)
    {
        transmitter_control_st.transmitter_key_encryption_pars.key_fragment = 0;
    }
}

void save_last_pdu_timestamp()
{
    transmitter_control_st.transmitter_key_encryption_pars.last_pdu_timestamp = transmitter_control_st.transmitter_key_encryption_pars.encrypted_key_pdu.key_data.timestamp;
}

void start_up_key_fragment_sending()
{
    transmitter_control_st.transmitter_key_encryption_pars.last_pdu_timestamp = 0;
    transmitter_control_st.transmitter_key_encryption_pars.key_data.timestamp = get_abs_timestamp_ms();
    uint32_t time_interval_ms = get_random_time_interval();
    transmitter_control_st.transmitter_key_encryption_pars.key_data.next_packet_arrival_ms = time_interval_ms;
    esp_timer_start_once(transmitter_control_st.key_fragment_schedule_st.timer, time_interval_ms * 1000);

    encrypt_key_fragment(
        transmitter_control_st.key_fragments.fragment[transmitter_control_st.transmitter_key_encryption_pars.key_fragment],
        transmitter_control_st.transmitter_key_encryption_pars.last_pdu_timestamp,
        transmitter_control_st.transmitter_key_encryption_pars.key_data.timestamp,
        transmitter_control_st.transmitter_key_encryption_pars.key_data.enc_key_fragment
    );


    calculate_hmac_with_decrypted_key_fragment(
        transmitter_control_st.key_fragments.fragment[transmitter_control_st.transmitter_key_encryption_pars.key_fragment],
        transmitter_control_st.transmitter_key_encryption_pars.last_pdu_timestamp,
        transmitter_control_st.transmitter_key_encryption_pars.key_data.timestamp,
        transmitter_control_st.transmitter_key_encryption_pars.key_data.enc_key_fragment,
        KEY_FRAGMENT_SIZE,
        transmitter_control_st.transmitter_key_encryption_pars.key_data.key_fragment_hmac
    );

    build_beacon_pdu_key(&transmitter_control_st.transmitter_key_encryption_pars.key_data, &transmitter_control_st.transmitter_key_encryption_pars.encrypted_key_pdu);
    save_last_pdu_timestamp();
    increment_next_key_fragment_index();
}

void encrypt_nexy_key_fragment(uint32_t time_interval_ms)
{
    transmitter_control_st.transmitter_key_encryption_pars.key_data.timestamp = get_abs_timestamp_ms();
    transmitter_control_st.transmitter_key_encryption_pars.key_data.next_packet_arrival_ms = time_interval_ms;
    encrypt_key_fragment(
        transmitter_control_st.key_fragments.fragment[transmitter_control_st.transmitter_key_encryption_pars.key_fragment],
        transmitter_control_st.transmitter_key_encryption_pars.last_pdu_timestamp,
        transmitter_control_st.transmitter_key_encryption_pars.key_data.timestamp,
        transmitter_control_st.transmitter_key_encryption_pars.key_data.enc_key_fragment
    );

    calculate_hmac_with_decrypted_key_fragment(
        transmitter_control_st.key_fragments.fragment[transmitter_control_st.transmitter_key_encryption_pars.key_fragment],
        transmitter_control_st.transmitter_key_encryption_pars.last_pdu_timestamp,
        transmitter_control_st.transmitter_key_encryption_pars.key_data.timestamp,
        transmitter_control_st.transmitter_key_encryption_pars.key_data.enc_key_fragment,
        KEY_FRAGMENT_SIZE,
        transmitter_control_st.transmitter_key_encryption_pars.key_data.key_fragment_hmac
    );

    build_beacon_pdu_key(&transmitter_control_st.transmitter_key_encryption_pars.key_data, &transmitter_control_st.transmitter_key_encryption_pars.encrypted_key_pdu);
    save_last_pdu_timestamp();
    increment_next_key_fragment_index();
}