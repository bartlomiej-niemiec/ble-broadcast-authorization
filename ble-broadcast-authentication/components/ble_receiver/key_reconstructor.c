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

#define MAX_ELEMENTS_IN_QUEUE 15

#define QUEUE_TIMEOUT_MS 20

typedef struct{
    uint16_t key_id;    
    uint8_t key_fragment_no;
    uint8_t encrypted_key_fragment[KEY_FRAGMENT_SIZE];
    uint8_t key_hmac[HMAC_SIZE];
    uint8_t xor_seed;
} __attribute__((aligned(4))) reconstructor_queue_element;;

static const char* RECONSTRUCTION_TASK_NAME = "KEY_RECONSTRUCTION_TASK";
static const char* REC_LOG_GROUP = "RECONSTRUCTION TASK";
static TaskHandle_t *xRecontructionKeyTask = NULL;
static QueueHandle_t xQueueKeyReconstruction;
static bool is_reconstructor_resources_init = false;
static volatile key_reconstruciton_cb key_rec_cb = NULL;

void reconstructor_main(void *arg);

void register_callback_to_key_reconstruction(key_reconstruciton_cb cb)
{
    ESP_LOGI(REC_LOG_GROUP, "Callback has been registered");
    key_rec_cb = cb;
}

bool init_reconstructor_resources()
{
    if (is_reconstructor_resources_init == false)
    {
        xQueueKeyReconstruction = xQueueCreate(MAX_ELEMENTS_IN_QUEUE, sizeof(reconstructor_queue_element));

        if (xQueueKeyReconstruction == NULL)
        {
            ESP_LOGE(REC_LOG_GROUP, "Failed to create PDU Queue!");
        }
        else
        {
            is_reconstructor_resources_init = true;
        }   
    }

    return is_reconstructor_resources_init;
}


int queue_key_for_reconstruction(uint16_t key_id, uint8_t key_fragment_no, uint8_t * encrypted_key_fragment, uint8_t * key_hmac, uint8_t xor_seed)
{
    BaseType_t result = pdFAIL;

    if (is_reconstructor_resources_init == true)
    {
        reconstructor_queue_element q_in = {};
        q_in.key_id = key_id;
        q_in.key_fragment_no = key_fragment_no;
        q_in.xor_seed = xor_seed;

        memcpy(q_in.encrypted_key_fragment, encrypted_key_fragment, KEY_FRAGMENT_SIZE);
        memcpy(q_in.key_hmac, key_hmac, HMAC_SIZE);

        result = xQueueSend(xQueueKeyReconstruction, (void *)&q_in, (TickType_t)0);
        
        // if (result == pdPASS) {
        //     ESP_LOGI(REC_LOG_GROUP, "Queue send successful for key_id: %d", key_id);
        // } else {
        //     ESP_LOGE(REC_LOG_GROUP, "Queue send failed for key_id: %d", key_id);
        // }
    }

    return result;
}



void start_up_key_reconstructor(void) {

    bool init_success = init_reconstructor_resources();

    if (init_success == true)
    {
        if (xRecontructionKeyTask == NULL)
        {
            BaseType_t  taskCreateResult = xTaskCreatePinnedToCore(
                reconstructor_main,
                RECONSTRUCTION_TASK_NAME, 
                (uint32_t) KEY_RECONSTRUCTION_TASK_SIZE,
                NULL,
                (UBaseType_t) RECONSTRUCTOR_TASK_PRIORITY,
                xRecontructionKeyTask,
                RECONSTRUCTOR_TASK_CORE
                );
        
            if (taskCreateResult != pdPASS)
            {
                ESP_LOGE(REC_LOG_GROUP, "Task was not created successfully! :(");
            }
            else
            {
                ESP_LOGI(REC_LOG_GROUP, "Task was created successfully! :)");
            }
        
        }
    }
}


void reconstructor_main(void *arg)
{
    ESP_LOGI(REC_LOG_GROUP, "Starting up key reconstructor task");
    const TickType_t TASK_DELAY_TICKS = pdMS_TO_TICKS(TASK_DELAY_MS);
    reconstructor_queue_element q_element;
    uint8_t decrypted_key_fragment_buffer[KEY_FRAGMENT_SIZE] = {};
    uint8_t calculated_hmac_buffer[HMAC_SIZE] = {};
    key_128b reconstructed_key_cache = {};
    uint8_t reconstructed_key_id_cache = 0;
    while (1)
    {
        if (xQueueReceive(xQueueKeyReconstruction, ( void * ) &q_element, QUEUE_TIMEOUT_MS / portTICK_PERIOD_MS) == pdTRUE)
        {
            if (is_key_in_collection(q_element.key_id) == false)
            {
                add_new_key_to_collection(q_element.key_id);
                ESP_LOGI(REC_LOG_GROUP, "Adding new key to collection for reconstruction: %i", q_element.key_id);
            }

            if (is_key_fragment_decrypted(q_element.key_id, q_element.key_fragment_no) == false)
            {
                ESP_LOGI(REC_LOG_GROUP, "Trying to reconstruct key fragment: %i", q_element.key_fragment_no);
                memset(decrypted_key_fragment_buffer, 0, sizeof(decrypted_key_fragment_buffer));
                memset(calculated_hmac_buffer, 0, sizeof(calculated_hmac_buffer));

                xor_decrypt_key_fragment(q_element.encrypted_key_fragment, decrypted_key_fragment_buffer, q_element.xor_seed);

                calculate_hmac_of_fragment(decrypted_key_fragment_buffer, q_element.encrypted_key_fragment, calculated_hmac_buffer);

                if (memcmp(calculated_hmac_buffer, q_element.key_hmac, sizeof(calculated_hmac_buffer)) == 0)
                {
                    add_fragment_to_key_management(q_element.key_id, decrypted_key_fragment_buffer, q_element.key_fragment_no);
                    ESP_LOGI(REC_LOG_GROUP, "Successfully reconstructed key fragment no: %i", q_element.key_fragment_no);
                }
            }
            
            if (is_key_available(q_element.key_id) == true)
            {
                if (reconstructed_key_id_cache == 0)
                {
                    reconstructed_key_id_cache = q_element.key_id;
                    reconstruct_key_from_key_fragments(&reconstructed_key_cache, q_element.key_id);
                }

                if (key_rec_cb != NULL)
                {
                    key_rec_cb(reconstructed_key_id_cache, &reconstructed_key_cache);
                }
                else
                {
                    ESP_LOGI(REC_LOG_GROUP, "Callback is not registered to be called");
                }
            }

        }

        memset(&q_element, 0, sizeof(reconstructor_queue_element));
        vTaskDelay(TASK_DELAY_TICKS);
    }
}




