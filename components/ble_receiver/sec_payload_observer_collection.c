#include "freertos/FreeRTOS.h"
#include "sec_payload_observer_collection.h"
#include "esp_log.h"

static const char * PDO_LOG_GROUP = "PAYLOAD_OBSERVER_COLLECTION";

payload_decrypted_observer_collection * create_pdo_collection(const size_t collection_size)
{
    payload_decrypted_observer_collection * p_doc = NULL;
    p_doc = (payload_decrypted_observer_collection *) malloc(sizeof(payload_decrypted_observer_collection));
    if (p_doc == NULL)
    {
        ESP_LOGI(PDO_LOG_GROUP, "Failed to malloc mem for payload_decrypted_observer_collection");
        return p_doc;
    }

    p_doc->collection_size = collection_size;
    p_doc->observers = (payload_decrypted_observer_cb *) calloc(collection_size, sizeof(payload_decrypted_observer_cb));
    
    if (p_doc->observers == NULL)
    {
        ESP_LOGI(PDO_LOG_GROUP, "Failed to malloc mem for payload_decrypted_observer_cb array");
        free(p_doc);
        return NULL;
    }

    p_doc->xMutex = xSemaphoreCreateMutex();
    if (p_doc->xMutex == NULL)
    {
        ESP_LOGI(PDO_LOG_GROUP, "Failed to malloc mem for Mutex");
        for (int i = 0; i < collection_size; i++)
            free(p_doc->observers[i]);
        free(p_doc);
        return NULL;
    }

    return p_doc;
}

int add_observer_to_collection(payload_decrypted_observer_collection * colletion, payload_decrypted_observer_cb observer)
{
    int status = -1;
    if (colletion == NULL || observer == NULL){
        ESP_LOGI(PDO_LOG_GROUP, "NULL ptr passed to add_observer_to_collection()");
        return status;
    }

    if (xSemaphoreTake(colletion->xMutex, portMAX_DELAY) == pdTRUE)
    {
        int i;
        for (i = 0; i < colletion->collection_size; i++)
        {
            if (colletion->observers[i] == NULL)
            {
                colletion->observers[i] = observer;
                break;
            }
        }

        status = i < colletion->collection_size ? 0 : -2;
        xSemaphoreGive(colletion->xMutex);
    }
    else
    {
        ESP_LOGI(PDO_LOG_GROUP, "Failed to obtained Mutex in add_observer_to_collection()");
    }

    return status;
}

void notify_pdo_collection_observers(payload_decrypted_observer_collection * colletion, uint8_t * decrypted_payload, size_t payload_size, esp_bd_addr_t mac_address)
{
    if (decrypted_payload == NULL || payload_size == 0 || mac_address == NULL)
        return;

    if (xSemaphoreTake(colletion->xMutex, portMAX_DELAY) == pdTRUE)
    {
        for (int i = 0; i < colletion->collection_size; i++)
        {
            if (colletion->observers[i] != NULL)
            {
                colletion->observers[i](decrypted_payload, payload_size, mac_address);
            }
        }
        xSemaphoreGive(colletion->xMutex);
    }
    else
    {
        ESP_LOGI(PDO_LOG_GROUP, "Failed to obtained Mutex in notify_observers()");
    }

}