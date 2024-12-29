#include "crypto/crypto.h"
#include <stdio.h>
#include <string.h>
#include "esp_crc.h"
#include "mbedtls/aes.h"
#include "mbedtls/gcm.h"
#include "mbedtls/md.h"
#include "esp_random.h"

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


int aes_ctr_encrypt_payload(uint8_t *input, size_t length, uint8_t *key, uint8_t *nonce, uint8_t *output) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) {
        mbedtls_aes_free(&aes);
        return -1; // Error: Failed to set AES key
    }

    size_t nc_off = 0; // Offset for the stream block
    uint8_t stream_block[16] = {0};

    if (mbedtls_aes_crypt_ctr(&aes, length, &nc_off, nonce, stream_block, input, output) != 0) {
        mbedtls_aes_free(&aes);
        return -2; // Error: AES CTR encryption failed
    }

    mbedtls_aes_free(&aes);
    return 0; // Success
}

int aes_ctr_decrypt_payload(uint8_t *input, size_t length, uint8_t *key, uint8_t *nonce, uint8_t *output) {
    // Decryption in CTR mode is identical to encryption
    return aes_ctr_encrypt_payload(input, length, key, nonce, output);
}

int aes_ctr_encrypt_fragment(uint8_t *key_fragment, uint8_t *aes_key, uint8_t *aes_iv, uint8_t *encrypted_fragment) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    if (mbedtls_aes_setkey_enc(&aes, aes_key, 128) != 0) {
        mbedtls_aes_free(&aes);
        return -1; // Error
    }

    size_t nc_off = 0;
    uint8_t stream_block[16] = {0};

    if (mbedtls_aes_crypt_ctr(&aes, 4, &nc_off, aes_iv, stream_block, key_fragment, encrypted_fragment) != 0) {
        mbedtls_aes_free(&aes);
        return -2; // Error
    }

    mbedtls_aes_free(&aes);
    return 0; // Success
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

void derive_aes_ctr_key_iv(uint32_t time_interval, uint32_t session_id, uint8_t *nonce, uint8_t *aes_key, uint8_t *aes_iv) {
    uint8_t data[12]; // time_interval (4) + session_id (4) + nonce (4)
    memcpy(&data[0], &time_interval, 4);
    memcpy(&data[4], &session_id, 4);
    memcpy(&data[8], nonce, 4);

    // Derive AES key and IV using HMAC
    calculate_hmac(NULL, 0, data, sizeof(data), aes_key); // Generate 16-byte key
    memcpy(aes_iv, aes_key + 8, 8);                       // Use part of the HMAC as IV
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