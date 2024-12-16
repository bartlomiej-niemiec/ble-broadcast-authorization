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
#define MAX_DEFERRED_QUEUE_ELEMENTS 100
#define MAX_PROCESSING_QUEUE_ELEMENTS 20
#define KEY_CACHE_SIZE 3
#define SEC_PROCESSING_TASK_DELAY_MS 30
#define SEC_PROCESSING_TASK_DELAY_SYS_TICKS pdMS_TO_TICKS(SEC_PROCESSING_TASK_DELAY_MS)

#define QUEUE_TIMEOUT_MS 40

#define DELAY_PROCESS_DEFERRED_Q 10

static const char * SEC_PDU_PROC_LOG = "SEC_PDU_PROCESSING";
static const char* SEC_PROCESSING_TASK_NAME = "SEC_PDU_PROCESSING TASK";

// Event group flags
#define EVENT_NEW_PDU (1 << 0)
#define EVENT_KEY_RECONSTRUCTED (1 << 1)
#define EVENT_PROCESS_DEFFERRED_PDUS (1 << 2)

typedef struct {
    TaskHandle_t xSecProcessingTask;
    QueueHandle_t deferredQueue;
    QueueHandle_t processingQueue;
    EventGroupHandle_t eventGroup;
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

int process_deferred_queue(const key_128b * const key, uint16_t max_processed_packets);
void decrypt_and_print_msg(const key_128b * const key, beacon_pdu_data * pdu);
int add_to_deferred_queue(beacon_pdu_data* pdu);
int init_sec_processing_resources();
bool is_pdu_in_deferred_queue();



void sec_processing_main(void *arg)
{
    beacon_pdu_data pdu;
    const key_128b * key = NULL;
    const uint16_t MAX_PROCESSED_PDUS_AT_ONCE = 10;
    uint8_t key_id = 0;
    while (1)
    {
        // Wait for events: either a new PDU or key reconstruction completion
        EventBits_t events = xEventGroupWaitBits(sec_pdu_st.eventGroup,
                                                 EVENT_NEW_PDU | EVENT_KEY_RECONSTRUCTED,
                                                 pdTRUE, pdFALSE, portMAX_DELAY);


        if (events & EVENT_NEW_PDU) {
            int counter = 0;
            bool packets_in_deferred_queue = is_pdu_in_deferred_queue();
            while (xQueueReceive(sec_pdu_st.processingQueue, (void *)&pdu, 0) == pdTRUE && MAX_PROCESSED_PDUS_AT_ONCE > counter++) {
                if (key == NULL && key_id != pdu.bcd.key_id) {
                    queue_key_for_reconstruction(pdu.bcd.key_id, pdu.bcd.key_fragment_no, 
                                                  pdu.bcd.enc_key_fragment, pdu.bcd.key_fragment_hmac, 
                                                  pdu.bcd.xor_seed);
                    add_to_deferred_queue(&pdu);
                }
                else if (key != NULL && key_id == pdu.bcd.key_id && packets_in_deferred_queue == true)
                {
                    add_to_deferred_queue(&pdu);
                }
                else
                {
                    decrypt_and_print_msg(key, &pdu);
                }
                vTaskDelay(pdMS_TO_TICKS(DELAY_PROCESS_DEFERRED_Q));
            }

            if (key != NULL && key_id == pdu.bcd.key_id && packets_in_deferred_queue == true)
            {
                xEventGroupSetBits(sec_pdu_st.eventGroup, EVENT_PROCESS_DEFFERRED_PDUS);
            }
        }

        if (events & EVENT_KEY_RECONSTRUCTED) {

            key = get_key_from_cache(sec_pdu_st.key_cache, pdu.bcd.key_id);
            key_id = pdu.bcd.key_id;
            process_deferred_queue(key, MAX_PROCESSED_PDUS_AT_ONCE);
            if (is_pdu_in_deferred_queue() == true)
            {
                xEventGroupSetBits(sec_pdu_st.eventGroup, EVENT_PROCESS_DEFFERRED_PDUS);
            }
        }


        if (events & EVENT_PROCESS_DEFFERRED_PDUS)
        {
            process_deferred_queue(key, MAX_PROCESSED_PDUS_AT_ONCE);
            if (is_pdu_in_deferred_queue() == true)
            {
                xEventGroupSetBits(sec_pdu_st.eventGroup, EVENT_PROCESS_DEFFERRED_PDUS);
            }
        }

    }
}

bool is_pdu_in_deferred_queue()
{
    beacon_pdu_data pdu;
    return xQueuePeek(sec_pdu_st.deferredQueue, ( void * ) &pdu, QUEUE_TIMEOUT_MS / portTICK_PERIOD_MS) == pdTRUE;
}

int process_deferred_queue(const key_128b * const key, uint16_t max_processed_packets)
{
    if (is_pdu_in_deferred_queue() == false)
    {
        return 0;
    }

    uint8_t nonce[NONCE_SIZE] = {0};
    uint8_t output[MAX_PDU_PAYLOAD_SIZE] = {0};
    beacon_pdu_data pdu;
    int counter = 0;
    while (xQueueReceive(sec_pdu_st.deferredQueue, (void *)&pdu, 0) == pdTRUE && max_processed_packets > counter++) {
        build_nonce(nonce, &(pdu.marker), pdu.bcd.key_fragment_no, pdu.bcd.key_id, pdu.bcd.xor_seed);
        aes_ctr_decrypt_payload(pdu.payload, sizeof(pdu.payload), key->key, nonce, output);
        ESP_LOGI(SEC_PDU_PROC_LOG, "STRING MESSAGE DECRYPTED: %s", (char *) output);
    }

    return counter;
}

void decrypt_and_print_msg(const key_128b * const key, beacon_pdu_data * pdu)
{;
    uint8_t nonce[NONCE_SIZE] = {0};
    uint8_t output[MAX_PDU_PAYLOAD_SIZE] = {0};
    build_nonce(nonce, &(pdu->marker), pdu->bcd.key_fragment_no, pdu->bcd.key_id, pdu->bcd.xor_seed);
    aes_ctr_decrypt_payload(pdu->payload, sizeof(pdu->payload), key->key, nonce, output);
    ESP_LOGI(SEC_PDU_PROC_LOG, "STRING MESSAGE DECRYPTED: %s", (char *) output);
}


int add_to_deferred_queue(beacon_pdu_data* pdu)
{
    BaseType_t stats = pdFAIL;
    ESP_LOGI(SEC_PDU_PROC_LOG, "Adding data to deffered queue");
    if (sec_pdu_st.is_sec_pdu_processing_initialised == true)
    {
        if (pdu != NULL)
        {
            stats = xQueueSend(sec_pdu_st.deferredQueue, ( void * ) pdu, ( TickType_t ) 0 );
        }
    }

    return stats;
}

void key_reconstruction_complete(uint8_t key_id, const key_128b * const reconstructed_key)
{
    int status = add_key_to_cache(sec_pdu_st.key_cache, reconstructed_key, key_id);
    if (status == 0)
    {
        ESP_LOGI(SEC_PDU_PROC_LOG, "Key has been added to the cache!");
        xEventGroupSetBits(sec_pdu_st.eventGroup, EVENT_KEY_RECONSTRUCTED);
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

    sec_pdu_st.eventGroup = xEventGroupCreate();

    if (sec_pdu_st.processingQueue == NULL)
    {
        vQueueDelete(sec_pdu_st.deferredQueue);
        vQueueDelete(sec_pdu_st.processingQueue);
        status = -3;
        return status;
    }


    int key_cache_status = 0;

    if ((key_cache_status = create_key_cache(&(sec_pdu_st.key_cache), sec_pdu_st.key_cache_size)) != 0 )
    {
        status = -4;
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
            status = -5;
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
    {   
        if (pdu != NULL)
        {
            stats = xQueueSend(sec_pdu_st.processingQueue, ( void * ) pdu, ( TickType_t ) 0 );
        }

        if (stats != pdPASS)
        {
            ESP_LOGE(SEC_PDU_PROC_LOG, "Process Queue is full!");
        }
        else
        {
            xEventGroupSetBits(sec_pdu_st.eventGroup, EVENT_NEW_PDU);
        }
    }

    return stats;
}