#include <string.h>

#include "ble_broadcast_security_encryptor.h"
#include "beacon_pdu_helpers.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

extern beacon_marker my_marker;

static const char * ENCRYPTOR_TASK_NAME = "ENCRYPTOR TASK";
static const char * ENCRYPTOR_LOG_GROUP = "ENCRYPTOR TASK";

#define ENCRYPTOR_TASK_SIZE 4096
#define ENCRYPTOR_TASK_PRIORITY 7
#define ENCRYPTOR_TASK_CORE 1

#define EVENT_KEY_FRAGMENT_ENCRYPTION_REQUEST    (1 << 0)
#define SEMAPHORE_TIMEOUT_MS 10

typedef struct {
    bool isInitialized;
    TaskHandle_t xTaskHandle;
    EventGroupHandle_t eventGroup;
    key_encryption_parameters key_fragment_pars;
    key_fragment_encrypted_callback key_fragment_callback;
    beacon_pdu_data bpdu;
    beacon_key_data key_data;
    beacon_key_pdu bkpdu;
}  encryptor_engine_st;

static encryptor_engine_st encryptor_st = {
    .isInitialized = false,
    .key_fragment_callback = NULL,
};

void encrypt_key_fragment();

static void encryptor_task_main_function(void *arg)
{
    while (1)
    {
         // Wait for events: either a new PDU or key reconstruction completion
        EventBits_t events = xEventGroupWaitBits(encryptor_st.eventGroup,
                                                 EVENT_KEY_FRAGMENT_ENCRYPTION_REQUEST,
                                                 pdTRUE, pdFALSE, portMAX_DELAY);


        if (events & EVENT_KEY_FRAGMENT_ENCRYPTION_REQUEST)
        {
            encrypt_key_fragment();
            if (encryptor_st.key_fragment_callback)
            {
                encryptor_st.key_fragment_callback(&encryptor_st.bkpdu);
            }
        }

    }
}

void request_key_fragment_pdu_encryption(key_encryption_parameters * parameters)
{
    if (parameters != NULL)
    {
        memcpy(&encryptor_st.key_fragment_pars, parameters, sizeof(key_encryption_parameters));
        xEventGroupSetBits(encryptor_st.eventGroup, EVENT_KEY_FRAGMENT_ENCRYPTION_REQUEST);
    }
    else
    {
        ESP_LOGE(ENCRYPTOR_LOG_GROUP, "Cannot encrypt fragments - NULL parameters");
    }
}

void register_fragment_encrypted_callback(key_fragment_encrypted_callback callback)
{
    if (encryptor_st.key_fragment_callback == NULL)
    {
        encryptor_st.key_fragment_callback = callback;
    }
    else
    {
        ESP_LOGE(ENCRYPTOR_LOG_GROUP, "Callback to fragment encryption is already set");
    }
}

bool init_and_start_up_encryptor_task()
{
    if (encryptor_st.isInitialized == false)
    {

        encryptor_st.eventGroup = xEventGroupCreate();
        if (encryptor_st.eventGroup == NULL) {
            ESP_LOGE(ENCRYPTOR_LOG_GROUP, "Failed to create event group!");
            return false;
        }

        if (encryptor_st.xTaskHandle == NULL)
        {
            BaseType_t  taskCreateResult = xTaskCreatePinnedToCore(
                (TaskFunction_t) encryptor_task_main_function,
                ENCRYPTOR_TASK_NAME, 
                (uint32_t) ENCRYPTOR_TASK_SIZE,
                NULL,
                (UBaseType_t) ENCRYPTOR_TASK_PRIORITY,
                (TaskHandle_t * )&(encryptor_st.xTaskHandle),
                ENCRYPTOR_TASK_CORE
                );
            
            if (taskCreateResult != pdPASS)
            {
                encryptor_st.xTaskHandle = NULL;
                ESP_LOGE(ENCRYPTOR_LOG_GROUP, "Task was not created successfully! :(");
            }
            else
            {
                ESP_LOGI(ENCRYPTOR_LOG_GROUP, "Task was created successfully! :)");
            }
            
        }
        else
        {
            ESP_LOGW(ENCRYPTOR_LOG_GROUP, "Task already running!");
        }

        encryptor_st.isInitialized = true;
    }

    return encryptor_st.isInitialized;
}

 
void encrypt_key_fragment() {
    
    uint8_t nonce[NONCE_SIZE];
    uint8_t aes_key[16];
    uint8_t aes_iv[8];
    uint8_t encrypted_fragment[KEY_FRAGMENT_SIZE];
    uint8_t hmac[HMAC_SIZE];
    
    // Encode session data
    uint32_t session_data = encode_session_data(encryptor_st.key_fragment_pars.session_id, encryptor_st.key_fragment_pars.key_fragment_index);

    // Generate random time interval
    uint32_t time_interval_ms = encryptor_st.key_fragment_pars.time_interval_ms + encryptor_st.key_fragment_pars.last_time_interval_ms;

    // Generate random nonce
    fill_random_bytes(nonce, sizeof(nonce));

    // Derive AES key and IV
    derive_aes_ctr_key_iv(time_interval_ms, (uint8_t*)&session_data, nonce, aes_key, aes_iv);

    // Encrypt the key fragment
    if (aes_ctr_encrypt_fragment(encryptor_st.key_fragment_pars.key_fragments.fragment[encryptor_st.key_fragment_pars.key_fragment_index], aes_key, aes_iv, encrypted_fragment) != 0) {
        ESP_LOGE(ENCRYPTOR_LOG_GROUP, "Encryption failed for fragment %d", encryptor_st.key_fragment_pars.key_fragment_index);
        return;
    }

    // Calculate HMAC
    calculate_hmac_of_fragment(encrypted_fragment, (uint8_t *) &my_marker, PDU_KEY_RECONSTRUCTION, (uint8_t*)&session_data, nonce, hmac);

    // Populate key data structure
    memcpy(&(encryptor_st.key_data.session_data), &session_data, sizeof(session_data));
    memcpy(encryptor_st.key_data.nonce, nonce, sizeof(nonce));
    memcpy(encryptor_st.key_data.enc_key_fragment,encrypted_fragment, sizeof(encrypted_fragment));
    memcpy(encryptor_st.key_data.key_fragment_hmac, hmac, sizeof(hmac));

    // Build the PDU
    build_beacon_pdu_key(&encryptor_st.key_data, &encryptor_st.bkpdu);

}



