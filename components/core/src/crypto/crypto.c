#include "crypto/crypto.h"
#include <stdio.h>
#include <string.h>
#include "esp_crc.h"
#include "esp_log.h"
#include "mbedtls/aes.h"
#include "mbedtls/gcm.h"
#include "mbedtls/md.h"
#include "esp_random.h"

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


// Derive AES Key and IV from timestamps and session data
void derive_aes_key_iv(uint32_t prev_timestamp, uint32_t current_timestamp, uint32_t session_data, uint8_t *aes_key, uint8_t *aes_iv) {
    uint8_t input_buffer[sizeof(prev_timestamp) + sizeof(current_timestamp) + sizeof(session_data)];
    memcpy(input_buffer, &prev_timestamp, sizeof(prev_timestamp));
    memcpy(input_buffer + sizeof(prev_timestamp), &current_timestamp, sizeof(current_timestamp));
    memcpy(input_buffer + sizeof(prev_timestamp) + sizeof(current_timestamp), &session_data, sizeof(session_data));

    uint8_t hash_output[32]; // SHA-256 output
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, md_info, 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, input_buffer, sizeof(input_buffer));
    mbedtls_md_finish(&ctx, hash_output);
    mbedtls_md_free(&ctx);

    memcpy(aes_key, hash_output, AES_KEY_SIZE);  // First 16 bytes for AES Key
    memcpy(aes_iv, hash_output + AES_KEY_SIZE, AES_IV_SIZE);  // Next 16 bytes for AES IV
}

// Encrypt and generate HMAC
void encrypt_key_fragment(const uint8_t *key_fragment, size_t fragment_size, 
                          uint32_t prev_timestamp, uint32_t current_timestamp, 
                          uint32_t session_data, uint8_t *encrypted_fragment, uint8_t *hmac) {
    uint8_t aes_key[AES_KEY_SIZE];
    uint8_t aes_iv[AES_IV_SIZE];

    // Derive AES key and IV
    derive_aes_key_iv(prev_timestamp, current_timestamp, session_data, aes_key, aes_iv);

    // Encrypt the 4-byte key fragment
    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_enc(&aes_ctx, aes_key, AES_KEY_SIZE * 8);
    uint8_t iv[AES_IV_SIZE];
    memcpy(iv, aes_iv, AES_IV_SIZE);
    mbedtls_aes_crypt_ctr(&aes_ctx, fragment_size, &fragment_size, iv, NULL, key_fragment, encrypted_fragment);
    mbedtls_aes_free(&aes_ctx);

    // Prepare HMAC input: ciphertext + metadata
    uint8_t hmac_input[fragment_size + sizeof(prev_timestamp) + sizeof(current_timestamp)];
    memcpy(hmac_input, encrypted_fragment, fragment_size);
    memcpy(hmac_input + fragment_size, &prev_timestamp, sizeof(prev_timestamp));
    memcpy(hmac_input + fragment_size + sizeof(prev_timestamp), &current_timestamp, sizeof(current_timestamp));

    // Calculate HMAC and truncate to 4 bytes
    uint8_t full_hmac[32];
    mbedtls_md_context_t hmac_ctx;
    const mbedtls_md_info_t *hmac_md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_init(&hmac_ctx);
    mbedtls_md_setup(&hmac_ctx, hmac_md_info, 1); // HMAC enabled
    mbedtls_md_hmac_starts(&hmac_ctx, aes_key, AES_KEY_SIZE);
    mbedtls_md_hmac_update(&hmac_ctx, hmac_input, sizeof(hmac_input));
    mbedtls_md_hmac_finish(&hmac_ctx, full_hmac);
    mbedtls_md_free(&hmac_ctx);

    memcpy(hmac, full_hmac, HMAC_SIZE); // Use first 4 bytes of HMAC
}

bool decrypt_key_fragment(const uint8_t *encrypted_fragment, size_t fragment_size, 
                          uint32_t prev_timestamp, uint32_t current_timestamp, 
                          uint32_t session_data, const uint8_t *hmac_received, 
                          uint8_t *decrypted_fragment) {
    uint8_t aes_key[AES_KEY_SIZE];
    uint8_t aes_iv[AES_IV_SIZE];

    // Derive AES key and IV
    derive_aes_key_iv(prev_timestamp, current_timestamp, session_data, aes_key, aes_iv);

    // Decrypt the 4-byte encrypted key fragment
    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_enc(&aes_ctx, aes_key, AES_KEY_SIZE * 8);
    uint8_t iv[AES_IV_SIZE];
    memcpy(iv, aes_iv, AES_IV_SIZE);
    mbedtls_aes_crypt_ctr(&aes_ctx, fragment_size, &fragment_size, iv, NULL, encrypted_fragment, decrypted_fragment);
    mbedtls_aes_free(&aes_ctx);

    // Prepare HMAC input: ciphertext + metadata
    uint8_t hmac_input[fragment_size + sizeof(prev_timestamp) + sizeof(current_timestamp)];
    memcpy(hmac_input, encrypted_fragment, fragment_size);
    memcpy(hmac_input + fragment_size, &prev_timestamp, sizeof(prev_timestamp));
    memcpy(hmac_input + fragment_size + sizeof(prev_timestamp), &current_timestamp, sizeof(current_timestamp));

    // Calculate HMAC and truncate to 4 bytes
    uint8_t full_hmac[32];
    uint8_t hmac_calculated[HMAC_SIZE];
    mbedtls_md_context_t hmac_ctx;
    const mbedtls_md_info_t *hmac_md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_init(&hmac_ctx);
    mbedtls_md_setup(&hmac_ctx, hmac_md_info, 1); // HMAC enabled
    mbedtls_md_hmac_starts(&hmac_ctx, aes_key, AES_KEY_SIZE);
    mbedtls_md_hmac_update(&hmac_ctx, hmac_input, sizeof(hmac_input));
    mbedtls_md_hmac_finish(&hmac_ctx, full_hmac);
    mbedtls_md_free(&hmac_ctx);

    memcpy(hmac_calculated, full_hmac, HMAC_SIZE);

    // Compare calculated HMAC with received HMAC
    if (memcmp(hmac_calculated, hmac_received, HMAC_SIZE) != 0) {
        ESP_LOGE(CRYPTO_LOG_GROUP, "HMAC verification failed!");
        return false;
    }

    ESP_LOGI(CRYPTO_LOG_GROUP, "HMAC verified successfully.");
    return true;
}

// Encrypt payload
bool encrypt_payload(const uint8_t *key, const uint8_t *payload, size_t payload_size, 
                     const uint8_t *session_id, const uint8_t *nonce, 
                     uint8_t *encrypted_payload) {
    if (payload_size < 14 || payload_size > 17) {
        ESP_LOGE(CRYPTO_LOG_GROUP, "Invalid payload size. Must be 14-17 bytes.");
        return false;
    }

    uint8_t iv[AES_KEY_SIZE] = {0}; // 16-byte IV initialized to 0
    memcpy(iv, session_id, SESSION_ID_SIZE); // First 4 bytes of IV are the session ID
    memcpy(iv + SESSION_ID_SIZE, nonce, NONCE_SIZE); // Next 4 bytes are the nonce

    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_enc(&aes_ctx, key, AES_KEY_SIZE * 8);

    size_t offset = 0;
    uint8_t stream_block[AES_KEY_SIZE] = {0}; // Buffer for stream block

    mbedtls_aes_crypt_ctr(&aes_ctx, payload_size, &offset, iv, stream_block, payload, encrypted_payload);

    mbedtls_aes_free(&aes_ctx);

    ESP_LOGI(CRYPTO_LOG_GROUP, "Payload encrypted successfully.");
    return true;
}

// Decrypt payload
bool decrypt_payload(const uint8_t *key, const uint8_t *encrypted_payload, size_t payload_size, 
                     const uint8_t *session_id, const uint8_t *nonce, 
                     uint8_t *decrypted_payload) {
    if (payload_size < 14 || payload_size > 17) {
        ESP_LOGE(CRYPTO_LOG_GROUP, "Invalid payload size. Must be 14-17 bytes.");
        return false;
    }

    uint8_t iv[AES_KEY_SIZE] = {0}; // 16-byte IV initialized to 0
    memcpy(iv, session_id, SESSION_ID_SIZE); // First 4 bytes of IV are the session ID
    memcpy(iv + SESSION_ID_SIZE, nonce, NONCE_SIZE); // Next 4 bytes are the nonce

    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_enc(&aes_ctx, key, AES_KEY_SIZE * 8);

    size_t offset = 0;
    uint8_t stream_block[AES_KEY_SIZE] = {0}; // Buffer for stream block

    mbedtls_aes_crypt_ctr(&aes_ctx, payload_size, &offset, iv, stream_block, encrypted_payload, decrypted_payload);

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
