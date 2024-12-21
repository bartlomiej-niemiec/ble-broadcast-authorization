#ifndef SEC_PAYLOAD_OBSERVER_COLLECTION
#define SEC_PAYLOAD_OBSERVER_COLLECTION

#include "sec_payload_decrypted_observer.h"
#include "freertos/semphr.h"

typedef struct {
    uint8_t collection_size;
    payload_decrypted_observer_cb *observers;
    SemaphoreHandle_t xMutex;
} payload_decrypted_observer_collection;

payload_decrypted_observer_collection * create_pdo_collection(const size_t collection_size);

int add_observer_to_collection(payload_decrypted_observer_collection * colletion, payload_decrypted_observer_cb observer);

void notify_pdo_collection_observers(payload_decrypted_observer_collection * colletion, uint8_t * decrypted_payload, size_t payload_size, esp_bd_addr_t mac_address);

#endif