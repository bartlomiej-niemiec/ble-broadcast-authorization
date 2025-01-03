#include "crypto/crypto.h"
#include <stdio.h>
#include <string.h>
#include "esp_crc.h"
#include "esp_log.h"
#include "mbedtls/aes.h"
#include "mbedtls/gcm.h"
#include "mbedtls/md.h"
#include "esp_random.h"

#define HMAC_KEY_SIZE 16

static const char *CRYPTO_LOG_GROUP = "ENCRYPTION";

void generate_128b_key(key_128b * key)
{
     if (key == NULL) {
        return; // Handle null pointer gracefully
    }
    esp_fill_random( (void *) key->key, sizeof(key->key));
}


void split_128b_key_to_fragment(key_128b * key, key_splitted * key_splitted)
{
    if (key == NULL || key_splitted == NULL) {
        return; // Handle null pointer gracefully
    }

    for (int i = 0; i < NO_KEY_FRAGMENTS; i++)
    {
        memcpy(&key_splitted->fragment[i], &key->key[i * KEY_FRAGMENT_SIZE], KEY_FRAGMENT_SIZE);
    }
}

void get_128b_key_from_fragments(key_128b * key, key_splitted * key_splitted)
{
    if (key == NULL || key_splitted == NULL) {
        return; // Handle null pointer gracefully
    }

    for (int i = 0; i < NO_KEY_FRAGMENTS; i++)
    {
        memcpy(&key->key[i * KEY_FRAGMENT_SIZE], &key_splitted->fragment[i], KEY_FRAGMENT_SIZE);
    } 
}

void add_fragment_to_key_spliited(key_splitted * key_splitted, uint8_t *fragment, uint8_t fragment_index)
{
    memcpy(key_splitted->fragment[fragment_index], fragment, KEY_FRAGMENT_SIZE);
}


void derive_aes_key_from_timestamps(uint32_t prev_timestamp, uint32_t curr_timestamp, uint8_t *key_out) {
    uint8_t timestamps_combined[8];
    memcpy(timestamps_combined, &prev_timestamp, sizeof(prev_timestamp));
    memcpy(timestamps_combined + sizeof(prev_timestamp), &curr_timestamp, sizeof(curr_timestamp));

    // Derive AES key by padding or hashing (this example uses direct copy for simplicity)
    memset(key_out, 0, AES_KEY_SIZE);
    memcpy(key_out, timestamps_combined, sizeof(timestamps_combined));
}

// Encrypt a 4-byte key fragment
bool encrypt_key_fragment(const uint8_t *fragment, uint32_t prev_timestamp, uint32_t curr_timestamp, uint8_t *encrypted_fragment) {
    uint8_t aes_key[AES_KEY_SIZE];
    uint8_t iv[AES_KEY_SIZE] = {0}; // IV initialized to zeros
    size_t offset = 0;
    uint8_t stream_block[AES_KEY_SIZE] = {0};

    // Derive AES key from timestamps
    derive_aes_key_from_timestamps(prev_timestamp, curr_timestamp, aes_key);

    // Initialize AES context
    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);
    if (mbedtls_aes_setkey_enc(&aes_ctx, aes_key, AES_KEY_SIZE * 8) != 0) {
        ESP_LOGE(CRYPTO_LOG_GROUP, "Failed to set AES encryption key");
        mbedtls_aes_free(&aes_ctx);
        return false;
    }

    // Encrypt the fragment using AES-CTR
    int ret = mbedtls_aes_crypt_ctr(&aes_ctx, KEY_FRAGMENT_SIZE, &offset, iv, stream_block, fragment, encrypted_fragment);
    if (ret != 0) {
        ESP_LOGE(CRYPTO_LOG_GROUP, "mbedtls_aes_crypt_ctr failed: %d", ret);
        mbedtls_aes_free(&aes_ctx);
        return false;
    }

    mbedtls_aes_free(&aes_ctx);
    return true;
}

bool calculate_hmac_with_decrypted_key_fragment(const uint8_t *decrypted_fragment, 
                                                uint32_t prev_timestamp, 
                                                uint32_t curr_timestamp, 
                                                const uint8_t *data, 
                                                size_t data_size, 
                                                uint8_t *hmac_output) {
    uint8_t hmac_full_output[HMAC_KEY_SIZE]; // Full HMAC output
    uint8_t hmac_input[8 + data_size];       // Combine metadata and data

    // Combine data for HMAC input
    size_t offset = 0;
    memcpy(hmac_input + offset, &prev_timestamp, sizeof(prev_timestamp));
    offset += sizeof(prev_timestamp);
    memcpy(hmac_input + offset, &curr_timestamp, sizeof(curr_timestamp));
    offset += sizeof(curr_timestamp);
    memcpy(hmac_input + offset, data, data_size);
    offset += data_size;

    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *md_info;

    mbedtls_md_init(&ctx);  // Initialize context

    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL) {
        ESP_LOGE("HMAC", "Failed to get md info");
        return false;
    }

    if (mbedtls_md_setup(&ctx, md_info, 1) != 0) {
        ESP_LOGE("HMAC", "Failed to set up HMAC context");
        mbedtls_md_free(&ctx); // Free partially initialized context
        return false;
    }

    if (mbedtls_md_hmac_starts(&ctx, decrypted_fragment, KEY_FRAGMENT_SIZE) != 0 ||
        mbedtls_md_hmac_update(&ctx, hmac_input, offset) != 0 ||
        mbedtls_md_hmac_finish(&ctx, hmac_full_output) != 0) {
        ESP_LOGE("HMAC", "Failed to calculate HMAC");
        mbedtls_md_free(&ctx);
        return false;
    }

    mbedtls_md_free(&ctx);  // Free initialized context

    // Truncate HMAC to 4 bytes
    memcpy(hmac_output, hmac_full_output, HMAC_SIZE);

    return true;
}

// Decrypt a 4-byte key fragment
bool decrypt_key_fragment(const uint8_t *encrypted_fragment, uint32_t prev_timestamp, uint32_t curr_timestamp, uint8_t *decrypted_fragment) {
    uint8_t aes_key[AES_KEY_SIZE];
    uint8_t iv[AES_KEY_SIZE] = {0}; // IV initialized to zeros
    size_t offset = 0;
    uint8_t stream_block[AES_KEY_SIZE] = {0};

    // Derive AES key from timestamps
    derive_aes_key_from_timestamps(prev_timestamp, curr_timestamp, aes_key);

    // Initialize AES context
    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);
    if (mbedtls_aes_setkey_enc(&aes_ctx, aes_key, AES_KEY_SIZE * 8) != 0) {
        ESP_LOGE(CRYPTO_LOG_GROUP, "Failed to set AES decryption key");
        mbedtls_aes_free(&aes_ctx);
        return false;
    }

    // Decrypt the fragment using AES-CTR
    int ret = mbedtls_aes_crypt_ctr(&aes_ctx, KEY_FRAGMENT_SIZE, &offset, iv, stream_block, encrypted_fragment, decrypted_fragment);
    if (ret != 0) {
        ESP_LOGE(CRYPTO_LOG_GROUP, "mbedtls_aes_crypt_ctr failed: %d", ret);
        mbedtls_aes_free(&aes_ctx);
        return false;
    }

    mbedtls_aes_free(&aes_ctx);
    return true;
}

void process_received_fragment(const uint8_t *encrypted_fragment, 
                               uint32_t prev_timestamp, 
                               uint32_t curr_timestamp, 
                               const uint8_t *received_hmac) {
    uint8_t decrypted_fragment[KEY_FRAGMENT_SIZE];
    uint8_t calculated_hmac[HMAC_SIZE];

    // Decrypt the fragment
    if (!decrypt_key_fragment(encrypted_fragment, prev_timestamp, curr_timestamp, decrypted_fragment)) {
        ESP_LOGE("FRAGMENT", "Decryption failed");
        return;
    }

    // Calculate HMAC
    if (!calculate_hmac_with_decrypted_key_fragment(decrypted_fragment, prev_timestamp, curr_timestamp, encrypted_fragment, KEY_FRAGMENT_SIZE, calculated_hmac)) {
        ESP_LOGE("FRAGMENT", "Failed to calculate HMAC");
        return;
    }

    // Validate HMAC
    if (memcmp(calculated_hmac, received_hmac, HMAC_SIZE) == 0) {
        ESP_LOGI("FRAGMENT", "Key fragment validated successfully");
    } else {
        ESP_LOGE("FRAGMENT", "HMAC validation failed");
    }
}

// Encrypt payload
bool encrypt_payload(const uint8_t *key, const uint8_t *payload, size_t payload_size, 
                     const uint8_t *session_id, const uint8_t *nonce, 
                     uint8_t *encrypted_payload) {
    uint8_t iv[AES_KEY_SIZE] = {0}; // 16-byte IV initialized to 0
    memcpy(iv, session_id, SESSION_ID_SIZE); // First 4 bytes of IV are the session ID
    memcpy(iv + SESSION_ID_SIZE, nonce, NONCE_SIZE); // Next 4 bytes are the nonce

    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_enc(&aes_ctx, key, AES_KEY_SIZE * 8);

    size_t offset = 0; // Offset for CTR mode
    uint8_t stream_block[AES_KEY_SIZE] = {0}; // Stream block initialized to zeros

    int ret = mbedtls_aes_crypt_ctr(&aes_ctx, payload_size, &offset, iv, stream_block, payload, encrypted_payload);
    if (ret != 0) {
        ESP_LOGE(CRYPTO_LOG_GROUP, "mbedtls_aes_crypt_ctr failed: %d", ret);
        mbedtls_aes_free(&aes_ctx);
        return false;
    }

    mbedtls_aes_free(&aes_ctx);

    ESP_LOGI(CRYPTO_LOG_GROUP, "Payload encrypted successfully.");
    return true;
}

// Decrypt payload
bool decrypt_payload(const uint8_t *key, const uint8_t *encrypted_payload, size_t payload_size, 
                     const uint8_t *session_id, const uint8_t *nonce, 
                     uint8_t *decrypted_payload) {
    uint8_t iv[AES_KEY_SIZE] = {0}; // 16-byte IV initialized to 0
    memcpy(iv, session_id, SESSION_ID_SIZE); // First 4 bytes of IV are the session ID
    memcpy(iv + SESSION_ID_SIZE, nonce, NONCE_SIZE); // Next 4 bytes are the nonce

    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_enc(&aes_ctx, key, AES_KEY_SIZE * 8);

    size_t offset = 0; // Offset for CTR mode
    uint8_t stream_block[AES_KEY_SIZE] = {0}; // Stream block initialized to zeros

    int ret = mbedtls_aes_crypt_ctr(&aes_ctx, payload_size, &offset, iv, stream_block, encrypted_payload, decrypted_payload);
    if (ret != 0) {
        ESP_LOGE(CRYPTO_LOG_GROUP, "mbedtls_aes_crypt_ctr failed: %d", ret);
        mbedtls_aes_free(&aes_ctx);
        return false;
    }

    mbedtls_aes_free(&aes_ctx);

    ESP_LOGI(CRYPTO_LOG_GROUP, "Payload decrypted successfully.");
    return true;
}


uint8_t get_random_seed()
{
    uint8_t seed;
    esp_fill_random(&seed, sizeof(seed));
    return seed;
}

void fill_random_bytes(uint8_t *arr, size_t size_arr)
{
    // Generate random nonce
    esp_fill_random(arr, size_arr);
}

void calculate_hmac(const uint8_t *key, size_t key_len, const uint8_t *message, size_t message_len, uint8_t *output)
{
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *md_info;
    int ret;

    // Initialize the HMAC context
    mbedtls_md_init(&ctx);

    // Select the hash function (e.g., SHA-256)
    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL) {
        printf("Failed to get hash algorithm info\n");
        return;
    }

    // Start HMAC with the chosen hash function
    ret = mbedtls_md_setup(&ctx, md_info, 1); // '1' means HMAC
    if (ret != 0) {
        printf("HMAC setup failed, error code: %d\n", ret);
        mbedtls_md_free(&ctx);
        return;
    }

    // Set the HMAC key
    ret = mbedtls_md_hmac_starts(&ctx, key, key_len);
    if (ret != 0) {
        printf("HMAC starts failed, error code: %d\n", ret);
        mbedtls_md_free(&ctx);
        return;
    }

    // Add the message
    ret = mbedtls_md_hmac_update(&ctx, message, message_len);
    if (ret != 0) {
        printf("HMAC update failed, error code: %d\n", ret);
        mbedtls_md_free(&ctx);
        return;
    }

    // Finalize and write the HMAC output
    ret = mbedtls_md_hmac_finish(&ctx, output);
    if (ret != 0) {
        printf("HMAC finish failed, error code: %d\n", ret);
    }

    // Free the HMAC context
    mbedtls_md_free(&ctx);
}

void calculate_hmac_of_fragment(uint8_t *encrypted_fragment, uint8_t *marker, uint8_t pdu_type, uint8_t *session_data, uint8_t *nonce, uint8_t *hmac_output)
{
    uint8_t data[23]; // Marker (6) + PDU Type (1) + Encrypted Fragment (4) + Session Data (4) + Nonce (4)
    memcpy(&data[0], marker, 6);
    data[6] = pdu_type;
    memcpy(&data[7], encrypted_fragment, 4);
    memcpy(&data[11], session_data, 4);
    memcpy(&data[15], nonce, 4);

    // Calculate HMAC
    calculate_hmac(NULL, 0, data, sizeof(data), hmac_output);
}

// Constant-time memory comparison
int crypto_secure_memcmp(const void *a, const void *b, size_t size)
{
    const uint8_t *p1 = (const uint8_t *)a;
    const uint8_t *p2 = (const uint8_t *)b;
    uint8_t result = 0;

    for (size_t i = 0; i < size; i++) {
        result |= p1[i] ^ p2[i]; // XOR and accumulate differences
    }

    return result == 0 ? 0 : 1; // Return 0 if identical, 1 otherwise
}

void build_key_frament_nonce(uint8_t *nonce, uint8_t nonce_size, uint32_t curr_timestamp, uint32_t prev_timestamp, uint16_t prev_next_arrival_time_ms, uint16_t curr_next_arrival_time_ms)
{

}
