#include <stddef.h>
#include <string.h>

#include "crypto.h"
#include "key_management.h"
#include "key_reconstructor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"

#define KEY_RECONSTRUCTION_TASK_SIZE 4096
#define RECONSTRUCTOR_TASK_CORE 1
#define RECONSTRUCTOR_TASK_PRIORITY 10

#define TASK_DELAY_MS 100
#define TASK_DELAY_SYSTICK (pdMS_TO_TICKS(TASK_DELAY_MS))

#define MAX_ELEMENTS_IN_QUEUE 15

#define QUEUE_TIMEOUT_MS 20

#define KEY_CACHE_MAP_SIZE 4

typedef struct{
    uint16_t key_id;    
    uint8_t key_fragment_no;
    uint8_t encrypted_key_fragment[KEY_FRAGMENT_SIZE];
    uint8_t key_hmac[HMAC_SIZE];
    uint8_t xor_seed;
} __attribute__((aligned(4))) reconstructor_queue_element;;

static const char* RECONSTRUCTION_TASK_NAME = "KEY_RECONSTRUCTION_TASK";
static const char* REC_LOG_GROUP = "RECONSTRUCTION TASK";

typedef struct {
    TaskHandle_t *xRecontructionKeyTask;
    QueueHandle_t xQueueKeyReconstruction;
    volatile bool is_reconstructor_resources_init;
    key_reconstruction_complete_cb key_rec_cb;
} key_reconstructor_control;

static key_reconstructor_control st_reconstructor_control = {
    .xRecontructionKeyTask = NULL,
    .xQueueKeyReconstruction = NULL,
    .is_reconstructor_resources_init = false,
    .key_rec_cb = NULL
};

void process_and_store_key_fragment(reconstructor_queue_element * q_element);
bool init_reconstructor_resources();

void reconstructor_main(void *arg)
{
    ESP_LOGI(REC_LOG_GROUP, "Starting up key reconstructor task");

    reconstructor_queue_element q_element;

    while (1)
    {
        if (xQueueReceive(st_reconstructor_control.xQueueKeyReconstruction, ( void * ) &q_element, QUEUE_TIMEOUT_MS / portTICK_PERIOD_MS) == pdTRUE)
        {
            if (is_key_in_collection(q_element.key_id) == false)
            {
                add_new_key_to_collection(q_element.key_id);
                ESP_LOGI(REC_LOG_GROUP, "Adding new key to collection for reconstruction: %i", q_element.key_id);
            }

            if (is_key_fragment_decrypted(q_element.key_id, q_element.key_fragment_no) == false)
            {
                process_and_store_key_fragment(&q_element);
            }

            if (is_key_available(q_element.key_id) == true)
            {
                key_128b reconstructed_key = {0};
                reconstruct_key_from_key_fragments(&reconstructed_key, q_element.key_id);
                if (st_reconstructor_control.key_rec_cb != NULL)
                {
                    st_reconstructor_control.key_rec_cb(q_element.key_id, &reconstructed_key);
                }
                remove_key_from_collection(q_element.key_id);
            }
        }

        vTaskDelay(TASK_DELAY_SYSTICK);
    }
}

int start_up_key_reconstructor(void) {

    int status = 0;
    st_reconstructor_control.is_reconstructor_resources_init = init_reconstructor_resources();

    if (st_reconstructor_control.is_reconstructor_resources_init == true)
    {
        if (st_reconstructor_control.xRecontructionKeyTask == NULL)
        {
            BaseType_t  taskCreateResult = xTaskCreatePinnedToCore(
                reconstructor_main,
                RECONSTRUCTION_TASK_NAME, 
                (uint32_t) KEY_RECONSTRUCTION_TASK_SIZE,
                NULL,
                (UBaseType_t) RECONSTRUCTOR_TASK_PRIORITY,
                st_reconstructor_control.xRecontructionKeyTask,
                RECONSTRUCTOR_TASK_CORE
                );
        
            if (taskCreateResult != pdPASS) {
                status = -2;
                ESP_LOGE(REC_LOG_GROUP, "Task was not created successfully! :(");
                // Clean up queue and mutex to avoid leaks
                vQueueDelete(st_reconstructor_control.xQueueKeyReconstruction);
                st_reconstructor_control.is_reconstructor_resources_init = false;
            } else {
                ESP_LOGI(REC_LOG_GROUP, "Task was created successfully! :)");
            }
        
        }
    }
    else
    {
        status = -1;
    }

    return status;
}

void register_callback_to_key_reconstruction(key_reconstruction_complete_cb cb)
{
    ESP_LOGI(REC_LOG_GROUP, "Callback has been registered");
    st_reconstructor_control.key_rec_cb = cb;
}

bool init_reconstructor_resources()
{
    bool queue_init_success = false;

    st_reconstructor_control.xQueueKeyReconstruction = xQueueCreate(MAX_ELEMENTS_IN_QUEUE, sizeof(reconstructor_queue_element));

    if (st_reconstructor_control.xQueueKeyReconstruction == NULL) {
        ESP_LOGE(REC_LOG_GROUP, "Failed to create PDU Queue!");
        return false;
    } else {
        queue_init_success = true;
    }

    if (!queue_init_success) {
        vQueueDelete(st_reconstructor_control.xQueueKeyReconstruction);
        return false;
    }

    return true;
}


RECONSTRUCTION_QUEUEING_STATUS queue_key_for_reconstruction(uint16_t key_id, uint8_t key_fragment_no, uint8_t * encrypted_key_fragment, uint8_t * key_hmac, uint8_t xor_seed)
{
    RECONSTRUCTION_QUEUEING_STATUS result = QUEUED_SUCCESS;

    if (st_reconstructor_control.is_reconstructor_resources_init == true)
    {
        if (is_key_available(key_id) == true)
        {
            ESP_LOGI(REC_LOG_GROUP, "Key ID %d already in cache, skipping queue.", key_id);
            return QUEUED_FAILED_KEY_ALREADY_RECONSTRUCTED; // Key already reconstructed       
        }

        if (encrypted_key_fragment == NULL || key_hmac == NULL) {
            ESP_LOGE(REC_LOG_GROUP, "Invalid parameters in %s", __func__);
            result = QUEUED_FAILED_BAD_INPUT;
        }
        else
        {
            if (is_key_fragment_decrypted(key_id, key_fragment_no) == false)
            {
                reconstructor_queue_element q_in = {};
                q_in.key_id = key_id;
                q_in.key_fragment_no = key_fragment_no;
                q_in.xor_seed = xor_seed;

                memcpy(q_in.encrypted_key_fragment, encrypted_key_fragment, KEY_FRAGMENT_SIZE);
                memcpy(q_in.key_hmac, key_hmac, HMAC_SIZE);

                int queue_result = xQueueSend(st_reconstructor_control.xQueueKeyReconstruction, (void *)&q_in, (TickType_t)0);
                
                if (queue_result != pdPASS) {
                    ESP_LOGE(REC_LOG_GROUP, "Queue send failed for key_id: %d", key_id);
                    result = QUEUED_FAILED_NO_SPACE;
                }
            }
            else
            {
                ESP_LOGE(REC_LOG_GROUP, "Key fragment %i is already decrypted", key_fragment_no);
            }
        }
    }
    else
    {
        ESP_LOGE(REC_LOG_GROUP, "Queue has not been initialized!");
        result = QUEEUD_FAILED_NOT_INITIALIZED;
    }

    return result;
}

void process_and_store_key_fragment(reconstructor_queue_element * q_element)
{
    ESP_LOGI(REC_LOG_GROUP, "Trying to reconstruct key fragment: %i", q_element->key_fragment_no);
    uint8_t decrypted_key_fragment_buffer[KEY_FRAGMENT_SIZE] = {0};
    uint8_t calculated_hmac_buffer[HMAC_SIZE] = {0};

    xor_decrypt_key_fragment(q_element->encrypted_key_fragment, decrypted_key_fragment_buffer, q_element->xor_seed);

    calculate_hmac_of_fragment(decrypted_key_fragment_buffer, q_element->encrypted_key_fragment, calculated_hmac_buffer);

    if (memcmp(calculated_hmac_buffer, q_element->key_hmac, sizeof(calculated_hmac_buffer)) == 0)
    {
        add_fragment_to_key_management(q_element->key_id, decrypted_key_fragment_buffer, q_element->key_fragment_no);
        ESP_LOGI(REC_LOG_GROUP, "Successfully reconstructed key fragment no: %i", q_element->key_fragment_no);
    }
}
