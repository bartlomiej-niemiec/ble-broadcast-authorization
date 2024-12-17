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

#include <limits.h>

#define SEC_PROCESSING_TASK_SIZE 4096
#define SEC_PROCESSING_TASK_PRIORITY 5
#define SEC_PROCESSING_TASK_CORE 1
#define MAX_DEFERRED_QUEUE_ELEMENTS 100
#define MAX_PROCESSING_QUEUE_ELEMENTS 100
#define KEY_CACHE_SIZE 3

#define MAX_PROCESSED_PDUS_AT_ONCE 20

#define QUEUE_TIMEOUT_MS 50
#define QUEUE_TIMEOUT_SYS_TICKS pdMS_TO_TICKS(QUEUE_TIMEOUT_MS)

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
    bool is_sec_pdu_processing_initialised;
    uint8_t deferred_queue_count;
} sec_pdu_processing_control;

static sec_pdu_processing_control sec_pdu_st = {
    .xSecProcessingTask = NULL,
    .deferredQueue = NULL,
    .processingQueue = NULL,
    .eventGroup = NULL,
    .key_cache = NULL,
    .key_cache_size = KEY_CACHE_SIZE,
    .is_sec_pdu_processing_initialised = false,
    .deferred_queue_count = 0
};

int process_deferred_queue_new_key();
int process_deferred_queue();
void decrypt_and_print_msg(const key_128b * const key, beacon_pdu_data * pdu);
int add_to_deferred_queue(beacon_pdu_data* pdu);
int init_sec_processing_resources();
void handle_event_new_pdu();
void handle_event_key_reconstructed();
void handle_event_process_deferred_pdus();
bool is_pdu_in_deferred_queue();
void decrement_deferred_queue_counter();
void increment_deferred_queue_counter();

void sec_processing_main(void *arg)
{

    while (1)
    {
        // Wait for events: either a new PDU or key reconstruction completion
        EventBits_t events = xEventGroupWaitBits(sec_pdu_st.eventGroup,
                                                 EVENT_NEW_PDU | EVENT_KEY_RECONSTRUCTED | EVENT_PROCESS_DEFFERRED_PDUS,
                                                 pdTRUE, pdFALSE, portMAX_DELAY);

        if (events & EVENT_KEY_RECONSTRUCTED) {
            handle_event_key_reconstructed();
        }

        if (events & EVENT_PROCESS_DEFFERRED_PDUS)
        {
            handle_event_process_deferred_pdus();
        }

        if (events & EVENT_NEW_PDU) {
            handle_event_new_pdu();
        }

    }
}

void handle_event_new_pdu()
{
    int batchCount = 0;
    beacon_pdu_data pduBatch[MAX_PROCESSED_PDUS_AT_ONCE] = {0};
    while (batchCount < MAX_PROCESSED_PDUS_AT_ONCE &&
        xQueueReceive(sec_pdu_st.processingQueue, (void *)&pduBatch[batchCount], 0) == pdTRUE)
    {
        batchCount++;
    }

    const key_128b * key = NULL;

    bool is_pdu_in_deferred_q = is_pdu_in_deferred_queue();

    for (int i = 0; i < batchCount; i++)
    {
        key = get_key_from_cache(sec_pdu_st.key_cache, pduBatch[i].bcd.key_id);
        if (key == NULL ) {
            queue_key_for_reconstruction(pduBatch[i].bcd.key_id, pduBatch[i].bcd.key_fragment_no, 
                                        pduBatch[i].bcd.enc_key_fragment, pduBatch[i].bcd.key_fragment_hmac, 
                                        pduBatch[i].bcd.xor_seed);
            add_to_deferred_queue(&pduBatch[i]);
        }
        else if (key != NULL && is_pdu_in_deferred_q)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            add_to_deferred_queue(&pduBatch[i]);
        }
        else
        {
            decrypt_and_print_msg(key, &pduBatch[i]);
        }
            
    }

    if (is_pdu_in_deferred_q)
    {
        xEventGroupSetBits(sec_pdu_st.eventGroup, EVENT_PROCESS_DEFFERRED_PDUS);
    }
}

void handle_event_key_reconstructed()
{
    process_deferred_queue();
    if (is_pdu_in_deferred_queue())
    {
        xEventGroupSetBits(sec_pdu_st.eventGroup, EVENT_PROCESS_DEFFERRED_PDUS);
    }
}

void handle_event_process_deferred_pdus()
{
    process_deferred_queue();
    if (is_pdu_in_deferred_queue())
    {
        xEventGroupSetBits(sec_pdu_st.eventGroup, EVENT_PROCESS_DEFFERRED_PDUS);
    }
}

int process_deferred_queue()
{
    beacon_pdu_data pduBatch[MAX_PROCESSED_PDUS_AT_ONCE] = {0};

    int counter = 0;
    while (xQueueReceive(sec_pdu_st.deferredQueue, (void *)&pduBatch[counter], QUEUE_TIMEOUT_SYS_TICKS) == pdTRUE && MAX_PROCESSED_PDUS_AT_ONCE > counter) {
        decrement_deferred_queue_counter();
        counter++;
    }

    for (int i = 0; i < counter; i++)
    {
        const key_128b *key = get_key_from_cache(sec_pdu_st.key_cache, pduBatch[i].bcd.key_id);
        if (key != NULL)
        {
            decrypt_and_print_msg(key, &pduBatch[i]);
        }
        else
        {
            add_to_deferred_queue(&pduBatch[i]);
        }
    }

    return counter;
}

inline bool is_pdu_in_deferred_queue()
{
    return sec_pdu_st.deferred_queue_count > 0;
}

inline void increment_deferred_queue_counter()
{
    if (sec_pdu_st.deferred_queue_count < UCHAR_MAX)
    {
        sec_pdu_st.deferred_queue_count++;
    }
}

inline void decrement_deferred_queue_counter()
{
    if (sec_pdu_st.deferred_queue_count > 0)
    {
        sec_pdu_st.deferred_queue_count--;
    }
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
            stats = xQueueSend(sec_pdu_st.deferredQueue, ( void * ) pdu, QUEUE_TIMEOUT_SYS_TICKS);
            if (stats == pdPASS) {
                increment_deferred_queue_counter();
            } else {
                ESP_LOGE(SEC_PDU_PROC_LOG, "Deferred queue is full!");
            }
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
            stats = xQueueSend(sec_pdu_st.processingQueue, ( void * ) pdu, QUEUE_TIMEOUT_SYS_TICKS);
        }
        xEventGroupSetBits(sec_pdu_st.eventGroup, EVENT_NEW_PDU);
    }

    return stats;
}