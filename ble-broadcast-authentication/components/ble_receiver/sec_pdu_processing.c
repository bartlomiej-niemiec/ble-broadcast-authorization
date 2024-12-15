#include "beacon_pdu_data.h"
#include "sec_pdu_processing.h"
#include "key_reconstructor.h"
#include "key_cache.h"
#include "crypto.h"

//FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#define SEC_PROCESSING_TASK_SIZE 4096
#define SEC_PROCESSING_TASK_PRIORITY 5
#define SEC_PROCESSING_TASK_CORE 1
#define MAX_DEFERRED_QUEUE_ELEMENTS 40
#define MAX_PROCESSING_QUEUE_ELEMENTS 20
#define KEY_CACHE_SIZE 3
#define SEC_PROCESSING_TASK_DELAY_MS 30
#define SEC_PROCESSING_TASK_DELAY_SYS_TICKS pdMS_TO_TICKS(SEC_PROCESSING_TASK_DELAY_MS)

#define QUEUE_TIMEOUT_MS 40

static const char * SEC_PDU_PROC_LOG = "SEC_PDU_PROCESSING";
static const char* SEC_PROCESSING_TASK_NAME = "SEC_PDU_PROCESSING TASK";

typedef struct {
    TaskHandle_t xSecProcessingTask;
    QueueHandle_t deferredQueue;
    QueueHandle_t processingQueue;
    key_reconstruction_cache *key_cache;
    const uint8_t key_cache_size;
    volatile bool is_sec_pdu_processing_initialised;
} sec_pdu_processing_control;

static sec_pdu_processing_control sec_pdu_st = {
    .xSecProcessingTask = NULL,
    .deferredQueue = NULL,
    .processingQueue = NULL,
    .key_cache = NULL,
    .key_cache_size = KEY_CACHE_SIZE,
    .is_sec_pdu_processing_initialised = false
};

int processes_deferred_queue(const uint8_t max_processes_elements_at_once, key_128b * key);
void decrypt_and_print_msg(key_128b * key, beacon_pdu_data * pdu);
int add_to_deferred_queue(beacon_pdu_data* pdu);
int init_sec_processing_resources();

void sec_processing_main(void *arg)
{
    beacon_pdu_data pdu;
    const uint8_t MAX_PROCESSED_DEFERRED_PDUS_AT_ONCE = 10;
    key_128b key = {0};
    uint8_t key_id = 0;
    while (1)
    {
        if (xQueueReceive(sec_pdu_st.processingQueue, ( void * ) &pdu, QUEUE_TIMEOUT_MS / portTICK_PERIOD_MS) == pdTRUE)
        {
            if (key_id == 0)
            {
                if (is_key_in_cache(sec_pdu_st.key_cache, pdu.bcd.key_id) == true)
                {
                    get_key_from_cache(sec_pdu_st.key_cache, &key, pdu.bcd.key_id);
                    key_id = pdu.bcd.key_id;
                }
                else
                {
                    RECONSTRUCTION_QUEUEING_STATUS reconstructor_queue_stat = queue_key_for_reconstruction(pdu.bcd.key_id, pdu.bcd.key_fragment_no, pdu.bcd.enc_key_fragment, pdu.bcd.key_fragment_hmac, pdu.bcd.xor_seed);
                    if (reconstructor_queue_stat != QUEUED_SUCCESS)
                    {
                        ESP_LOGI(SEC_PDU_PROC_LOG, "Key reconstruction queued failed!: %i", reconstructor_queue_stat);
                    }
                    BaseType_t result = add_to_deferred_queue(&pdu);
                    if (result != pdPASS)
                    {
                        ESP_LOGE(SEC_PDU_PROC_LOG, "Deferred Queue is full!");
                    }
                }
            }

            if (key_id == pdu.bcd.key_id)
            {
                int processed_elements = processes_deferred_queue(MAX_PROCESSED_DEFERRED_PDUS_AT_ONCE, &key);
                if (processed_elements == 0)
                {
                    decrypt_and_print_msg(&key, &pdu);
                }
                else
                {
                    add_to_deferred_queue(&pdu);
                }
            }
        }

        vTaskDelay(SEC_PROCESSING_TASK_DELAY_SYS_TICKS);

    }
}

int processes_deferred_queue(const uint8_t max_processes_elements_at_once, key_128b * key)
{
    int counter = 0;
    beacon_pdu_data pdu;
    while (xQueueReceive(sec_pdu_st.processingQueue, ( void * ) &pdu, QUEUE_TIMEOUT_MS / portTICK_PERIOD_MS) == pdTRUE && counter < max_processes_elements_at_once)
    {
        ESP_LOGI(SEC_PDU_PROC_LOG, "Processing deferred msg no. %i", counter);
        decrypt_and_print_msg(key, &pdu);
        counter++;
    }

    return counter;
}


void decrypt_and_print_msg(key_128b * key, beacon_pdu_data * pdu)
{;
    uint8_t nonce[NONCE_SIZE] = {0};
    uint8_t output[MAX_PDU_PAYLOAD_SIZE] = {0};
    build_nonce(nonce, &(pdu->marker), pdu->bcd.key_fragment_no, pdu->bcd.key_id, pdu->bcd.xor_seed);
    aes_ctr_decrypt_payload(pdu->payload, sizeof(pdu->payload), key->key, nonce, output);
    ESP_LOG_BUFFER_HEX("MESSAGE DECRYPTED:", output, sizeof(output));
    ESP_LOGI(SEC_PDU_PROC_LOG, "STRING MESSAGE DECRYPTED: %s", (char *) output);
}


int add_to_deferred_queue(beacon_pdu_data* pdu)
{
    BaseType_t stats = pdFAIL;

    if (sec_pdu_st.is_sec_pdu_processing_initialised == true)
    {
        if (pdu != NULL)
        {
            stats = xQueueSend(sec_pdu_st.deferredQueue, ( void * ) pdu, ( TickType_t ) 0 );
        }
    }

    return stats;
}

void key_reconstruction_complete(uint8_t key_id, key_128b * const reconstructed_key)
{
    int status = add_key_to_cache(sec_pdu_st.key_cache, reconstructed_key, key_id);
    if (status == 0)
    {
        ESP_LOGI(SEC_PDU_PROC_LOG, "Key has been added to the cache!");
        ESP_LOGI(SEC_PDU_PROC_LOG, "Reconstructed key id: %i", key_id);
        ESP_LOG_BUFFER_HEX("KEY RECONSTRUCTED HEX", reconstructed_key, sizeof(key_128b));
    }
    else
    {
        ESP_LOGE(SEC_PDU_PROC_LOG, "Key has not been added to the cache!");
    }
}

int init_sec_processing_resources()
{
    int status = 0;

    //init deferred queue
    sec_pdu_st.deferredQueue = xQueueCreate(MAX_DEFERRED_QUEUE_ELEMENTS, sizeof(beacon_pdu_data));
    if (sec_pdu_st.deferredQueue == NULL)
    {
        status = -1;
        return status;
    }

    //init processingQueue
    sec_pdu_st.processingQueue = xQueueCreate(MAX_PROCESSING_QUEUE_ELEMENTS, sizeof(beacon_pdu_data));
    if (sec_pdu_st.processingQueue == NULL)
    {
        vQueueDelete(sec_pdu_st.deferredQueue);
        status = -2;
        return status;
    }


    int key_cache_status = 0;

    if ((key_cache_status = create_key_cache(&(sec_pdu_st.key_cache), sec_pdu_st.key_cache_size)) != 0 )
    {
        status = -3;
        vQueueDelete(sec_pdu_st.deferredQueue);
        vQueueDelete(sec_pdu_st.processingQueue);
        ESP_LOGE(SEC_PDU_PROC_LOG, "Key cache creation failed!: %i", key_cache_status);
    }
    else
    {
        ESP_LOGI(SEC_PDU_PROC_LOG, "Key cache: %p", sec_pdu_st.key_cache);
        if ((key_cache_status = init_key_cache(sec_pdu_st.key_cache)) != 0)
        {
            free(sec_pdu_st.key_cache->map);
            free(sec_pdu_st.key_cache);
            vQueueDelete(sec_pdu_st.deferredQueue);
            vQueueDelete(sec_pdu_st.processingQueue);
            status = -4;
            ESP_LOGE(SEC_PDU_PROC_LOG, "Key cache init failed!: %i", key_cache_status);
        }
    }

    return status;
}

int start_up_sec_processing()
{
    int status = 0;

    status = init_sec_processing_resources();

    if (status == 0)
    {
        int key_reconstructor_status = start_up_key_reconstructor();
        if (key_reconstructor_status != 0)
        {
            status = -3;
        }
        else
        {
            register_callback_to_key_reconstruction(key_reconstruction_complete);
        }
    }


    if (status == 0)
    {
        sec_pdu_st.is_sec_pdu_processing_initialised = true;
        if (sec_pdu_st.xSecProcessingTask == NULL)
        {
            BaseType_t  taskCreateResult = xTaskCreatePinnedToCore(
                sec_processing_main,
                SEC_PROCESSING_TASK_NAME, 
                (uint32_t) SEC_PROCESSING_TASK_SIZE,
                NULL,
                (UBaseType_t) SEC_PROCESSING_TASK_PRIORITY,
                &(sec_pdu_st.xSecProcessingTask),
                SEC_PROCESSING_TASK_CORE
                );
        
            if (taskCreateResult != pdPASS)
            {
                status = -2;
                ESP_LOGE(SEC_PDU_PROC_LOG, "Task was not created successfully! :(");
            }
            else
            {
                ESP_LOGI(SEC_PDU_PROC_LOG, "Task was created successfully! :)");
            }
        }
    }

    return status;
}


int process_sec_pdu(beacon_pdu_data* pdu)
{
    BaseType_t stats = pdFAIL;

    if (sec_pdu_st.is_sec_pdu_processing_initialised == true)
    {        if (pdu != NULL)
        {
            stats = xQueueSend(sec_pdu_st.processingQueue, ( void * ) pdu, ( TickType_t ) 0 );
        }

        if (stats != pdPASS)
        {
            ESP_LOGE(SEC_PDU_PROC_LOG, "Process Queue is full!");
        }
    }

    return stats;
}