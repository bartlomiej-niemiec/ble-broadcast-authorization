#include "crypto/crypto.h"
#include <stdio.h>
#include <string.h>
#include "esp_crc.h"
#include "mbedtls/aes.h"
#include "mbedtls/gcm.h"
#include "mbedtls/md.h"
#include "esp_random.h"
#include "esp_log.h"

static const char * crypto_log_group = "CRYPTO";

void generate_128b_key(key_128b * key)
{
     if (key == NULL) {
        return;
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
        return -1;
    }

    size_t nc_off = 0;
    uint8_t stream_block[16] = {0};

    if (mbedtls_aes_crypt_ctr(&aes, length, &nc_off, nonce, stream_block, input, output) != 0) {
        mbedtls_aes_free(&aes);
        return -2;
    }

    mbedtls_aes_free(&aes);
    return 0;
}

int aes_ctr_decrypt_payload(uint8_t *input, size_t length, uint8_t *key, uint8_t *nonce, uint8_t *output) {
    return aes_ctr_encrypt_payload(input, length, key, nonce, output);
}

void xor_encrypt_key_fragment(uint8_t fragment[KEY_FRAGMENT_SIZE], uint8_t encrypted_fragment[KEY_FRAGMENT_SIZE], uint8_t xor_seed) {
    encrypted_fragment[0] = fragment[0] ^ xor_seed;
    for (int i = 1; i < KEY_FRAGMENT_SIZE; i++) {
        encrypted_fragment[i] = fragment[i] ^ encrypted_fragment[i - 1];
    }
}

void xor_decrypt_key_fragment(uint8_t encrypted_fragment[KEY_FRAGMENT_SIZE], uint8_t decrypted_fragment[KEY_FRAGMENT_SIZE], uint8_t xor_seed) {
    decrypted_fragment[0] = encrypted_fragment[0] ^ xor_seed;
    for (int i = 1; i < KEY_FRAGMENT_SIZE; i++) {
        decrypted_fragment[i] = encrypted_fragment[i] ^ encrypted_fragment[i - 1];
    }
}


uint8_t get_random_seed()
{
    uint8_t seed;
    esp_fill_random(&seed, sizeof(seed));
    return seed;
}

void calculate_hmac(const uint8_t *key, size_t key_len, const uint8_t *message, size_t message_len, uint8_t *output)
{
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *md_info;
    int ret;

    // Inicjalizacja kontekstu mbedtls
    mbedtls_md_init(&ctx);

    // Wybranie funkcji skrótu SHA-256
    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (md_info == NULL) {
        ESP_LOGE(crypto_log_group, "Blad przy wybieraniu funkcji skrotu");
        return;
    }

    // Ustawienie funkcji skrótu
    ret = mbedtls_md_setup(&ctx, md_info, 1); // '1' oznacza HMAC
    if (ret != 0) {
        ESP_LOGE(crypto_log_group, "Blad przy konfiguracji HMAC");
        mbedtls_md_free(&ctx);
        return;
    }

    // Ustaw klucz HMAC
    ret = mbedtls_md_hmac_starts(&ctx, key, key_len);
    if (ret != 0) {
        ESP_LOGE(crypto_log_group, "Blad przy rozpoczeciu liczenia HMAC");
        mbedtls_md_free(&ctx);
        return;
    }

    // Dodaj wiadomość
    ret = mbedtls_md_hmac_update(&ctx, message, message_len);
    if (ret != 0) {
        ESP_LOGE(crypto_log_group, "Blad przy aktualizacji znacznika HMAC");
        mbedtls_md_free(&ctx);
        return;
    }

    // Zakończ wyliczenia i zapisz HMAC do bufora wyjściowego
    ret = mbedtls_md_hmac_finish(&ctx, output);
    if (ret != 0) {
        ESP_LOGE(crypto_log_group, "Blad przy zakonczeniu zapisywania HMAC do bufora wyjsciowego");
    }

    // Zwolnij zaalokowany kontekst
    mbedtls_md_free(&ctx);
}

void calculate_hmac_of_fragment(uint8_t *key_fragment, uint8_t *encrypted_fragment, uint8_t *hmac_output) {
    // Policz HMAC dla zaszyfrowanego fragmentu używająć odszyfrowanego fragmentu jako klucz
    calculate_hmac(key_fragment, KEY_FRAGMENT_SIZE, encrypted_fragment, KEY_FRAGMENT_SIZE, hmac_output);
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