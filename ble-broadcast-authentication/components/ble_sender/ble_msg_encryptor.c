#include <string.h>
#include <stdint.h>
#include "esp_log.h"
#include "ble_msg_encryptor.h"
#include "esp_random.h"
#include "crypto.h"

static const char* MSG_SENDER_LOG_GROUP = "MSG_ENCRYPTOR";
static key_128b pre_shared_key;
static key_splitted splitted_pre_shared_key;
static uint16_t key_id;
static uint8_t encrypt_payload_arr[MAX_PDU_PAYLOAD_SIZE] = {0};

uint16_t get_random_fragment_id()
{
    return (esp_random() % NO_KEY_FRAGMENTS);
}

bool init_payload_encryption()
{
    static bool isInitialized = false;
    if (isInitialized == false)
    {
        key_id = 1;
        for (int i = 0; i < KEY_SIZE; i ++)
        {
            memset(&pre_shared_key.key[i], 1, sizeof(uint8_t));
        }
        //generate_128b_key(&pre_shared_key);
        split_128b_key_to_fragment(&pre_shared_key, &splitted_pre_shared_key);
        isInitialized = true;
    }

    return isInitialized;
}

void build_nonce(uint8_t nonce[16], const beacon_marker* marker, uint8_t key_fragment_number, uint16_t key_id, uint8_t xor_seed)
{
    memset(nonce, 0, 16);

    // Copy the marker into the beginning of the nonce.
    memcpy(nonce, marker->marker, sizeof(beacon_marker));

    // Add the key ID (2 bytes) in little-endian format.
    nonce[sizeof(beacon_marker)]     = key_id & 0xFF;        // Low byte of key ID
    nonce[sizeof(beacon_marker) + 1] = (key_id >> 8) & 0xFF; // High byte of key ID

    // Add the key fragment number (1 byte).
    nonce[sizeof(beacon_marker) + 2] = key_fragment_number;

    // Add the XOR seed (1 byte).
    nonce[sizeof(beacon_marker) + 3] = xor_seed;

    // The remaining bytes of the nonce are already zero-filled from memset.
}



int encrypt_payload(uint8_t * payload, size_t payload_size, beacon_pdu_data * encrypted_pdu)
{
    if (payload_size > MAX_PDU_PAYLOAD_SIZE)
    {
        ESP_LOGE(MSG_SENDER_LOG_GROUP, "Payload size exceeds maximum allowed size");
        return 1;
    }

    const uint8_t key_fragment_no = get_random_fragment_id();
    const uint8_t random_xor_seed = get_random_seed();
    uint8_t nonce[16] = {0};

    encrypted_pdu->bcd.key_fragment_no = key_fragment_no;
    encrypted_pdu->bcd.key_id = key_id;
    encrypted_pdu->bcd.xor_seed = random_xor_seed;

    xor_encrypt_key_fragment(splitted_pre_shared_key.fragment[key_fragment_no], encrypted_pdu->bcd.enc_key_fragment, random_xor_seed);
    
    calculate_hmac_of_fragment(splitted_pre_shared_key.fragment[key_fragment_no], encrypted_pdu->bcd.enc_key_fragment, encrypted_pdu->bcd.key_fragment_hmac);

    build_nonce(nonce, &(encrypted_pdu->marker), key_fragment_no, key_id, random_xor_seed);


    memset(encrypt_payload_arr, 0, sizeof(encrypt_payload_arr));
    memcpy(encrypt_payload_arr, payload, payload_size);
    aes_ctr_encrypt_payload(encrypt_payload_arr, payload_size, pre_shared_key.key, nonce, encrypt_payload_arr);
    memcpy(encrypted_pdu->payload, encrypt_payload_arr, payload_size);
    return 0;
}


