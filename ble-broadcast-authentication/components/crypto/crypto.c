#include "crypto.h"
#include <string.h>
#include "esp_crc.h"
#include "mbedtls/aes.h"
#include "mbedtls/gcm.h"
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

uint16_t calculate_crc_for_key_fragment(key_splitted * key_splitted, uint8_t no_fragment)
{
    // Validate pointers
    if (key_splitted == NULL) {
        return 0; // Return 0 or another appropriate value for invalid input
    }

    // Validate fragment index
    if (no_fragment >= NO_KEY_FRAGMENTS) {
        return 0; // Return 0 if the fragment index is out of range
    }

    return esp_crc16_le(0xFFFF, key_splitted->fragment[no_fragment], KEY_FRAGMENT_SIZE);
}

void generate_iv(uint16_t pdu_id, uint64_t time_interval, uint8_t *iv) {
    uint8_t *iv_ptr = iv;

    // First 2 bytes of IV: Copy PDU ID (2 bytes)
    iv_ptr[0] = (uint8_t)(pdu_id >> 8);  // High byte of PDU ID
    iv_ptr[1] = (uint8_t)(pdu_id & 0xFF);  // Low byte of PDU ID

    // Fill the remaining part of the IV with the timestamp or custom time value
    iv_ptr[2] = (uint8_t)(time_interval >> 56);
    iv_ptr[3] = (uint8_t)(time_interval >> 48);
    iv_ptr[4] = (uint8_t)(time_interval >> 40);
    iv_ptr[5] = (uint8_t)(time_interval >> 32);
    iv_ptr[6] = (uint8_t)(time_interval >> 24);
    iv_ptr[7] = (uint8_t)(time_interval >> 16);
    iv_ptr[8] = (uint8_t)(time_interval >> 8);
    iv_ptr[9] = (uint8_t)(time_interval);  // Low byte of timestamp

    // For simplicity, the timestamp might only take the first 10 bytes of the IV
    // Fill the last 6 bytes with zeros (or any deterministic value if needed)
    memset(&iv_ptr[10], 0, 6);  // Optionally, you can fill the remaining bytes as needed
}

int encrypt_payload_aes_ctr(uint8_t *payload, size_t payload_size, uint8_t *key, uint8_t *encrypted_payload, uint8_t *iv) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    // Set the AES key for encryption
    if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) {
        mbedtls_aes_free(&aes);
        return -1; // Error: Failed to set AES key
    }

    uint8_t counter[AES_BLOCK_BYTES] = {0};
    uint8_t stream_block[AES_BLOCK_BYTES] = {0};

    // Copy the IV into the counter for the first block
    memcpy(counter, iv, AES_BLOCK_BYTES);

    // Process the payload using AES-CTR (encryption in stream mode)
    for (size_t i = 0; i < payload_size; i++) {
        if (i % AES_BLOCK_BYTES == 0) {
            // Encrypt the counter (AES block)
            if (mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, counter, stream_block) != 0) {
                mbedtls_aes_free(&aes);
                return -2; // Error: AES encryption failed
            }

            // Increment the counter for the next block (CTR mode)
            for (int j = AES_BLOCK_BYTES - 1; j >= 0; j--) {
                if (++counter[j] != 0) break;
            }
        }

        // XOR the encrypted stream block with the payload byte to encrypt it
        encrypted_payload[i] = payload[i] ^ stream_block[i % AES_BLOCK_BYTES];
    }

    mbedtls_aes_free(&aes);
    return 0; // Success
}

int decrypt_payload_aes_ctr(uint8_t *encrypted_payload, size_t payload_size, uint8_t *key, uint8_t *decrypted_payload, uint8_t *iv) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    // Set the AES key for decryption (AES-CTR is symmetric)
    if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) {
        mbedtls_aes_free(&aes);
        return -1; // Error: Failed to set AES key
    }

    uint8_t counter[AES_BLOCK_BYTES] = {0};
    uint8_t stream_block[AES_BLOCK_BYTES] = {0};

    // Copy the IV into the counter for the first block
    memcpy(counter, iv, AES_BLOCK_BYTES);

    // Decrypt the payload using AES-CTR
    for (size_t i = 0; i < payload_size; i++) {
        if (i % AES_BLOCK_BYTES == 0) {
            // Encrypt the counter (AES block) for the next stream block
            if (mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, counter, stream_block) != 0) {
                mbedtls_aes_free(&aes);
                return -2; // Error: AES encryption failed
            }

            // Increment the counter for the next block
            for (int j = AES_BLOCK_BYTES - 1; j >= 0; j--) {
                if (++counter[j] != 0) break;
            }
        }

        // XOR the encrypted stream block with the encrypted payload byte to decrypt it
        decrypted_payload[i] = encrypted_payload[i] ^ stream_block[i % AES_BLOCK_BYTES];
    }

    mbedtls_aes_free(&aes);
    return 0; // Success
}
