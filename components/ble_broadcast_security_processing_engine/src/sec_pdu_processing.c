#include "ble_consumer_collection.h"
#include "ble_consumer.h"
#include "beacon_pdu_data.h"
#include "sec_payload_observer_collection.h"
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
#include <string.h>

#define SEC_PROCESSING_TASK_SIZE 4096
#define SEC_PROCESSING_TASK_PRIORITY 5
#define SEC_PROCESSING_TASK_CORE 1
#define MAX_PROCESSING_QUEUE_ELEMENTS 100
#define MAX_PROCESSED_PDUS_AT_ONCE 20

#define QUEUE_TIMEOUT_MS 50
#define QUEUE_TIMEOUT_SYS_TICKS pdMS_TO_TICKS(QUEUE_TIMEOUT_MS)

static const char * SEC_PDU_PROC_LOG = "SEC_PDU_PROCESSING";
static const char* SEC_PROCESSING_TASK_NAME = "SEC_PDU_PROCESSING TASK";

// Event group flags
#define EVENT_NEW_PDU (1 << 0)
#define EVENT_KEY_RECONSTRUCTED (1 << 1)
#define EVENT_PROCESS_DEFFERRED_PDUS (1 << 2)

#define MAIN_PROCESSING_QUEUE_SIZE

typedef struct {
    TaskHandle_t xSecProcessingTask;
    QueueHandle_t processingQueue;
    EventGroupHandle_t eventGroup;
    ble_consumer_collection* consumer_collection;
    size_t ble_consumer_collection_size;
    bool is_sec_pdu_processing_initialised;
    payload_decrypted_observer_collection * payload_decription_subcribers_collection;
} sec_pdu_processing_control;

static sec_pdu_processing_control sec_pdu_st = {
    .xSecProcessingTask = NULL,
    .processingQueue = NULL,
    .eventGroup = NULL,
    .consumer_collection = NULL,
    .ble_consumer_collection_size = MAX_BLE_CONSUMERS,
    .is_sec_pdu_processing_initialised = false,
    .payload_decription_subcribers_collection = NULL
};

typedef struct {
    beacon_pdu_data pdu;
    esp_bd_addr_t mac_address;
} beacon_pdu_data_with_mac_addr;

static int add_to_consumer_deferred_queue(ble_consumer* p_ble_consumer, beacon_pdu_data* pdu);
static int process_deferred_queue(ble_consumer * p_ble_consumer);
static void decrypt_pdu(const key_128b * const key, beacon_pdu_data * pdu, uint8_t * output, uint8_t output_len);
static void decrypt_and_notify(const key_128b *key, beacon_pdu_data *pdu, esp_bd_addr_t mac_address);
static int init_sec_processing_resources();
static void handle_event_new_pdu();
static void handle_event_process_deferred_pdus();

bool create_ble_broadcast_pdu_for_dispatcher(ble_broadcast_pdu* pdu, uint8_t *data, size_t size, esp_bd_addr_t mac_address)
{
    bool result = false;
    if (size <= MAX_GAP_DATA_LEN)
    {
        memcpy(pdu->data, data, size);
        memcpy(&(pdu->data_len), &size, sizeof(size));
        memcpy(&(pdu->mac_address), mac_address, sizeof(esp_bd_addr_t));
        result = true;
    }
    else
    {
        ESP_LOGE(SEC_PDU_PROC_LOG, "Size of data larger than pdu->data!");
    }
    return result;
}

void register_payload_observer_cb(payload_decrypted_observer_cb observer_cb)
{
    if (sec_pdu_st.is_sec_pdu_processing_initialised)
    {
        add_observer_to_collection(sec_pdu_st.payload_decription_subcribers_collection, observer_cb);
    }
}

void sec_processing_main(void *arg)
{

    while (1)
    {
        // Wait for events: either a new PDU or key reconstruction completion
        EventBits_t events = xEventGroupWaitBits(sec_pdu_st.eventGroup,
                                                 EVENT_NEW_PDU | EVENT_PROCESS_DEFFERRED_PDUS,
                                                 pdTRUE, pdFALSE, portMAX_DELAY);

        if (events & EVENT_NEW_PDU) {
            handle_event_new_pdu();
        }

        if (events & EVENT_PROCESS_DEFFERRED_PDUS)
        {
            handle_event_process_deferred_pdus();
        }

    }
}

void handle_event_new_pdu()
{
    int batchCount = 0;
    beacon_pdu_data_with_mac_addr pduBatch[MAX_PROCESSED_PDUS_AT_ONCE] = {0};
    while (batchCount < MAX_PROCESSED_PDUS_AT_ONCE &&
        xQueueReceive(sec_pdu_st.processingQueue, (void *)&pduBatch[batchCount], 0) == pdTRUE)
    {
        batchCount++;
    }

    const key_128b * key = NULL;
    ble_consumer * p_ble_consumer = NULL;

    for (int i = 0; i < batchCount; i++)
    {
        p_ble_consumer = get_ble_consumer_from_collection(sec_pdu_st.consumer_collection, pduBatch[i].mac_address);
        if (p_ble_consumer == NULL && get_active_no_consumers(sec_pdu_st.consumer_collection) < MAX_BLE_CONSUMERS)
        {
            p_ble_consumer = add_consumer_to_collection(sec_pdu_st.consumer_collection, pduBatch[i].mac_address);
            if (p_ble_consumer == NULL)
            {
                ESP_LOGE(SEC_PDU_PROC_LOG, "Failed adding new consumer to collection :(");
            }
            else
            {
                ESP_LOGI(SEC_PDU_PROC_LOG, "Successfully addded consumer to collection, count of active consumers: %i", get_active_no_consumers(sec_pdu_st.consumer_collection)); 
            }
        }

        if (p_ble_consumer == NULL)
        {
            ESP_LOGE(SEC_PDU_PROC_LOG, "Failed acquairing ble consumer from collection :(");
            continue;
        }

        // key = get_key_from_cache(p_ble_consumer->context.key_cache, pduBatch[i].pdu.bcd.key_id);
        if (key == NULL ) {
            // queue_key_for_reconstruction(pduBatch[i].pdu.bcd.key_id, pduBatch[i].pdu.bcd.key_fragment_no, 
            //                             pduBatch[i].pdu.bcd.enc_key_fragment, pduBatch[i].pdu.bcd.key_fragment_hmac, 
            //                             pduBatch[i].pdu.bcd.xor_seed, pduBatch[i].mac_address);
            add_to_consumer_deferred_queue(p_ble_consumer, &(pduBatch[i].pdu));
        }
        else if (is_pdu_in_deferred_queue(p_ble_consumer) > 0)
        {
            add_to_consumer_deferred_queue(p_ble_consumer, &(pduBatch[i].pdu));
        }
        else
        {
            decrypt_and_notify(key, &pduBatch[i].pdu, pduBatch[i].mac_address);
        }
            
    }
}

// Handle deferred PDUs event
static void handle_event_process_deferred_pdus() {
    ESP_LOGI(SEC_PDU_PROC_LOG, "Processing deferred queue...");
    for (size_t i = 0; i < sec_pdu_st.ble_consumer_collection_size; i++) {
        ble_consumer *consumer = &(sec_pdu_st.consumer_collection->arr[i]);
        if (consumer != NULL)
        {
            if (is_deferred_queue_request_pending(consumer)) {
                process_deferred_queue(consumer);
                if (!is_pdu_in_deferred_queue(consumer)) {
                    set_deferred_q_pending_processing(consumer, false);
                } else {
                    xEventGroupSetBits(sec_pdu_st.eventGroup, EVENT_PROCESS_DEFFERRED_PDUS);
                }
            }
        }
        else
        {
            ESP_LOGI(SEC_PROCESSING_TASK_NAME, "BLE Consumer NULL PTR");
        }
    }
}


// Decrypt PDU and notify callback
void decrypt_and_notify(const key_128b *key, beacon_pdu_data *pdu, esp_bd_addr_t mac_address) {
    uint8_t output[MAX_PDU_PAYLOAD_SIZE] = {0};
    decrypt_pdu(key, pdu, output, sizeof(output));
    notify_pdo_collection_observers(sec_pdu_st.payload_decription_subcribers_collection, output, MAX_PDU_PAYLOAD_SIZE, mac_address);
}

void decrypt_pdu(const key_128b * const key, beacon_pdu_data * pdu, uint8_t * output, uint8_t output_len)
{
    if (output_len == MAX_PDU_PAYLOAD_SIZE)
    {
        uint8_t nonce[NONCE_SIZE] = {0};
        // build_nonce(nonce, &(pdu->marker), pdu->bcd.key_fragment_no, pdu->bcd.key_id, pdu->bcd.xor_seed);
        aes_ctr_decrypt_payload(pdu->payload, sizeof(pdu->payload), key->key, nonce, output);
    }
}

int process_deferred_queue(ble_consumer * p_ble_consumer)
{
    beacon_pdu_data pduBatch[MAX_PROCESSED_PDUS_AT_ONCE] = {0};
    int counter = 0;
    while (get_deferred_queue_item(p_ble_consumer, &pduBatch[counter]) == pdTRUE && MAX_PROCESSED_PDUS_AT_ONCE > counter) {
        counter++;
    }
    
    //const key_128b *key = get_key_from_cache(p_ble_consumer->context.key_cache, pduBatch[0].bcd.key_id);
    //uint8_t last_key_id = pduBatch[0].bcd.key_id;
    for (int i = 0; i < counter; i++)
    {
        // if (last_key_id != pduBatch[i].bcd.key_id)
        // {
        //     key = get_key_from_cache(p_ble_consumer->context.key_cache, pduBatch[i].bcd.key_id);
        //     last_key_id = pduBatch[i].bcd.key_id;
        // }

        // if (key != NULL)
        // {
        //     decrypt_and_notify(key, &(pduBatch[i]), p_ble_consumer->mac_address_arr);
        // }
        // else
        // {   
        //     /// Drop PDU from removed key
        //     if (pduBatch[i].bcd.key_id != p_ble_consumer->context.recently_removed_key_id)
        //     {
        //         add_to_consumer_deferred_queue(p_ble_consumer, &pduBatch[i]);
        //     }
        // }
    }

    return counter;
}


int add_to_consumer_deferred_queue(ble_consumer* p_ble_consumer, beacon_pdu_data* pdu)
{
    BaseType_t stats = pdFAIL;
    ESP_LOGI(SEC_PDU_PROC_LOG, "Adding data to deffered queue");
    if (sec_pdu_st.is_sec_pdu_processing_initialised == true)
    {
        if (pdu != NULL)
        {
            add_to_deferred_queue(p_ble_consumer, pdu);
        }
    }

    return stats;
}

int update_key_in_cache(ble_consumer* p_ble_consumer, uint8_t key_id, const key_128b* reconstructed_key) {
    int status = add_key_to_cache(p_ble_consumer->context.key_cache, reconstructed_key, key_id);
    if (status != -1) {
        p_ble_consumer->context.recently_removed_key_id = remove_lru_key_from_cache(p_ble_consumer->context.key_cache);
        status = add_key_to_cache(p_ble_consumer->context.key_cache, reconstructed_key, key_id);
    }
    else if (status != -3)
    {
        ESP_LOGE(SEC_PDU_PROC_LOG, "add_key_to_cache null ptr");
    }
    return status;
}


void key_reconstruction_complete(uint8_t key_id, key_128b * const reconstructed_key, uint8_t *mac_address)
{
    // Retrieve BLE consumer associated with mac_address
    ble_consumer * p_ble_consumer = get_ble_consumer_from_collection(sec_pdu_st.consumer_collection, mac_address);
    if (p_ble_consumer == NULL)
    {
        ESP_LOGE(SEC_PDU_PROC_LOG, "BLE Consumer NULL for MAC address: %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]);
        return;
    }

    // Attempt to add the reconstructed key to the cache
    int status = add_key_to_cache(p_ble_consumer->context.key_cache, reconstructed_key, key_id);
    if (status == -1)
    {
        ESP_LOGE(SEC_PDU_PROC_LOG, "Removing key from cache");
        // Cache full; remove LRU and retry
        p_ble_consumer->context.recently_removed_key_id = remove_lru_key_from_cache(p_ble_consumer->context.key_cache);
        ESP_LOGE(SEC_PDU_PROC_LOG, "Removed key id: %i", p_ble_consumer->context.recently_removed_key_id);
        status = add_key_to_cache(p_ble_consumer->context.key_cache, reconstructed_key, key_id);
        if (status != 0)
        {
            ESP_LOGE(SEC_PDU_PROC_LOG, "Error adding new key: %i", status);
        }
    }
    
    if (status == 0)
    {
        // Key successfully added to cache
        ESP_LOGI(SEC_PDU_PROC_LOG, "Key added to cache for device: %02x:%02x:%02x:%02x:%02x:%02x, Key ID: %d",
                 mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5], key_id);

        // Mark deferred queue for processing
        set_deferred_q_pending_processing(p_ble_consumer, true);
        xEventGroupSetBits(sec_pdu_st.eventGroup, EVENT_PROCESS_DEFFERRED_PDUS);
    }
    else
    {
        ESP_LOGE(SEC_PDU_PROC_LOG, "Failed to add key to cache for device: %02x:%02x:%02x:%02x:%02x:%02x, Key ID: %d, Status: %d",
                 mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5], key_id, status);
    }
}

int init_sec_processing_resources()
{
    int status = 0;

    //init processingQueue
    sec_pdu_st.processingQueue = xQueueCreate(MAX_PROCESSING_QUEUE_ELEMENTS, sizeof(beacon_pdu_data_with_mac_addr));
    if (sec_pdu_st.processingQueue == NULL)
    {
        ESP_LOGE(SEC_PDU_PROC_LOG, "processing queue create failed!");
        status = -1;
        return status;
    }

    sec_pdu_st.eventGroup = xEventGroupCreate();

    if (sec_pdu_st.eventGroup == NULL) {
        status = -2;
        vEventGroupDelete(sec_pdu_st.eventGroup);
        vQueueDelete(sec_pdu_st.processingQueue);
        ESP_LOGE(SEC_PDU_PROC_LOG, "event group create failed!");
        return status;
    }

    sec_pdu_st.consumer_collection = create_ble_consumer_collection(sec_pdu_st.ble_consumer_collection_size, KEY_CACHE_SIZE);
    if (sec_pdu_st.consumer_collection == NULL)
    {
        status = -3;
        vQueueDelete(sec_pdu_st.processingQueue);
        vEventGroupDelete(sec_pdu_st.eventGroup);
        ESP_LOGE(SEC_PDU_PROC_LOG, "ble consumer collection create failed!");
    }

    sec_pdu_st.payload_decription_subcribers_collection = create_pdo_collection(MAX_OBSERVERS);
    if (sec_pdu_st.consumer_collection == NULL)
    {
        status = -4;
        vQueueDelete(sec_pdu_st.processingQueue);
        vEventGroupDelete(sec_pdu_st.eventGroup);
        destroy_ble_consumer_collection(sec_pdu_st.consumer_collection);
        ESP_LOGE(SEC_PDU_PROC_LOG, "ble consumer collection create failed!");
    }

    return status;
}

int start_up_sec_processing()
{
    int status = 0;

    status = init_sec_processing_resources();

    if (status == 0)
    {
        int key_reconstructor_status = start_up_key_reconstructor(MAX_BLE_CONSUMERS * 2);
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


int enqueue_pdu_for_processing(beacon_pdu_data* pdu, esp_bd_addr_t mac_address)
{
    BaseType_t stats = pdFAIL;

    if (sec_pdu_st.is_sec_pdu_processing_initialised == true)
    {   
        if (pdu != NULL && mac_address != NULL)
        {
            beacon_pdu_data_with_mac_addr temp_pdu;
            temp_pdu.pdu = *pdu;
            memcpy(temp_pdu.mac_address, mac_address, sizeof(esp_bd_addr_t));
            stats = xQueueSend(sec_pdu_st.processingQueue, ( void * ) &temp_pdu, QUEUE_TIMEOUT_SYS_TICKS);
            if (stats)
            {
                xEventGroupSetBits(sec_pdu_st.eventGroup, EVENT_NEW_PDU);
            }
        }
    }

    return stats;
}
