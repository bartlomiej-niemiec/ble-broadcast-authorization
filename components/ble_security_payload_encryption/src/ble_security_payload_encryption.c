#include "ble_security_payload_encryption.h"
#include "crypto.h"

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>
#include <stdint.h>
#include <limits.h>

#define KEY_REPLACEMENT_TIMEOUT_S 15
#define KEY_REPLACEMENT_TIMEOUT_US (KEY_REPLACEMENT_TIMEOUT_S * (1000000))

static const char* MSG_SENDER_LOG_GROUP = "MSG_ENCRYPTOR";
static key_128b pre_shared_key;
static key_splitted splitted_pre_shared_key;
static key_128b next_pre_shared_key;
static key_splitted next_splitted_pre_shared_key;
static volatile bool is_key_replace_request_active = false;
static uint16_t key_id;
static uint8_t key_time_interval_multiplier;
static uint8_t encrypt_payload_arr[MAX_PDU_PAYLOAD_SIZE] = {0};
static  esp_timer_handle_t key_replacement_timer;
void key_replacement_cb(void *arg);
static esp_timer_create_args_t key_replacement_timer_args = {
    .callback = key_replacement_cb,
    .arg = NULL,
    .name = "KEY REPLACEMENT TIMER",
};

void key_replacement_cb(void *arg)
{
    generate_128b_key(&next_pre_shared_key);
    ESP_LOG_BUFFER_HEX("New key: ", next_pre_shared_key.key, sizeof(next_pre_shared_key));
    split_128b_key_to_fragment(&next_pre_shared_key, &next_splitted_pre_shared_key);
    is_key_replace_request_active = true;
    esp_timer_start_once(key_replacement_timer, KEY_REPLACEMENT_TIMEOUT_US);
}

uint16_t get_random_fragment_id()
{
    return (esp_random() % NO_KEY_FRAGMENTS);
}

uint8_t get_random_time_interval_value()
{
    return (esp_random() % MAX_TIME_INTERVAL_MULTIPLIER);
}

uint16_t get_random_key_id()
{
    return ((esp_random() % (USHRT_MAX - 1)) + 1);
}

uint16_t get_random_pdus_count_to_incr_key_exchange_counter()
{
    const int BASE = 10;
    return (10 + (esp_random() % 20));
}

bool init_payload_encryption()
{
    static bool isInitialized = false;
    if (isInitialized == false)
    {
        key_id = get_random_key_id();
        key_time_interval_multiplier = get_random_time_interval_value();
        generate_128b_key(&pre_shared_key);
        ESP_LOG_BUFFER_HEX("New key: ", next_pre_shared_key.key, sizeof(pre_shared_key));
        split_128b_key_to_fragment(&pre_shared_key, &splitted_pre_shared_key);
        if (esp_timer_create(&key_replacement_timer_args, &key_replacement_timer) != ESP_OK)
        {
            return false;
        }
    }

    return isInitialized;
}


int encrypt_payload(uint8_t * payload, size_t payload_size, beacon_pdu_data * encrypted_pdu)
{
    static bool isTimerFirstStarted = false;
    if (isTimerFirstStarted == false)
    {
        esp_timer_start_once(key_replacement_timer, KEY_REPLACEMENT_TIMEOUT_US);
        isTimerFirstStarted = true;
    }

    if (payload_size > MAX_PDU_PAYLOAD_SIZE)
    {
        ESP_LOGE(MSG_SENDER_LOG_GROUP, "Payload size exceeds maximum allowed size");
        return 1;
    }

    if (is_key_replace_request_active == true)
    {
        ESP_LOGI(MSG_SENDER_LOG_GROUP, "Key replacement in progress...");
        key_id = get_random_key_id();
        memcpy(&pre_shared_key, &next_pre_shared_key, sizeof(pre_shared_key));
        memcpy(&splitted_pre_shared_key, &next_splitted_pre_shared_key, sizeof(pre_shared_key));
        is_key_replace_request_active = false;
    }

    const uint8_t key_fragment_no = get_random_fragment_id();
    const uint8_t random_xor_seed = get_random_seed();
    uint8_t nonce[NONCE_SIZE] = {0};

    uint16_t pdu_key_session_data = produce_key_session_data(key_id, key_fragment_no);
    encrypted_pdu->bcd.key_session_data = pdu_key_session_data;
    encrypted_pdu->bcd.key_exchange_data = produce_key_exchange_data(key_time_interval_multiplier, 0);
    encrypted_pdu->bcd.xor_seed = random_xor_seed;

    xor_encrypt_key_fragment(splitted_pre_shared_key.fragment[key_fragment_no], encrypted_pdu->bcd.enc_key_fragment, random_xor_seed);
        
    calculate_hmac_of_fragment(splitted_pre_shared_key.fragment[key_fragment_no], encrypted_pdu->bcd.enc_key_fragment, encrypted_pdu->bcd.key_fragment_hmac);

    build_nonce(nonce, &(encrypted_pdu->marker), pdu_key_session_data, key_time_interval_multiplier, random_xor_seed);

    uint8_t encrypt_payload_arr[MAX_PDU_PAYLOAD_SIZE] = {0};  // Local buffer for encryption
    memcpy(encrypt_payload_arr, payload, payload_size);

    uint8_t encrypted_payload[MAX_PDU_PAYLOAD_SIZE] = {0};  // Output buffer
    aes_ctr_encrypt_payload(encrypt_payload_arr, payload_size, pre_shared_key.key, nonce, encrypted_payload);

    memcpy(encrypted_pdu->payload, encrypted_payload, payload_size);
    encrypted_pdu->payload_size = payload_size;

    return 0;
}


