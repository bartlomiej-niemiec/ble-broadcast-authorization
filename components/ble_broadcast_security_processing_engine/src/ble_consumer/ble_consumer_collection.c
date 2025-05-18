#include "ble_consumer_collection.h"
#include "tick_count_timestamp.h"
#include "esp_log.h"
#include <string.h>

ble_consumer_collection * create_ble_consumer_collection(const uint8_t collection_size, const uint8_t sender_key_cache_size) {
    ble_consumer_collection *p_collection = (ble_consumer_collection *)malloc(sizeof(ble_consumer_collection));
    if (p_collection == NULL) {
        ESP_LOGE("BLE_COLLECTION", "Failed to allocate memory for BLE consumer collection!");
        return NULL;
    }

    p_collection->consumers_count = 0;
    p_collection->size = collection_size;
    p_collection->xMutex = xSemaphoreCreateMutex();

    if (p_collection->xMutex == NULL) {
        ESP_LOGE("BLE_COLLECTION", "Failed to create mutex!");
        free(p_collection);
        return NULL;
    }

    p_collection->arr = (ble_consumer *)malloc(sizeof(ble_consumer) * collection_size);
    if (p_collection->arr == NULL) {
        ESP_LOGE("BLE_COLLECTION", "Failed to allocate memory for BLE consumers!");
        vSemaphoreDelete(p_collection->xMutex);
        free(p_collection);
        return NULL;
    }

    for (int i = 0; i < collection_size; i++) {
        if (create_ble_consumer_resources(&p_collection->arr[i], sender_key_cache_size) != 0) {
            ESP_LOGE("BLE_COLLECTION", "Failed to create ble consumer resources!");
            for (int j = 0; j < i; j++) {
                destroy_ble_consumer(&p_collection->arr[j]);
            }
            free(p_collection->arr);
            vSemaphoreDelete(p_collection->xMutex);
            free(p_collection);
            return NULL;
        }

        if ( init_ble_consumer(&p_collection->arr[i]) != 0)
        {
            ESP_LOGE("BLE_COLLECTION", "Failed to init ble consumer resources!");
            for (int j = 0; j < i; j++) {
                destroy_ble_consumer(&p_collection->arr[j]);
            }
            free(p_collection->arr);
            vSemaphoreDelete(p_collection->xMutex);
            free(p_collection);
            return NULL;
        }
    }

    return p_collection;
}

void destroy_ble_consumer_collection(ble_consumer_collection * p_ble_consumer_collection)
{
    if (p_ble_consumer_collection != NULL)
    {
        if (p_ble_consumer_collection->xMutex)
        {
            vSemaphoreDelete(p_ble_consumer_collection->xMutex);
        }

        if (p_ble_consumer_collection->arr)
        {
            for (int i = p_ble_consumer_collection->size - 1; i >= 0; i--)
            {
                destroy_ble_consumer(&p_ble_consumer_collection->arr[i]);
            }
            free(p_ble_consumer_collection);
        }
    }
}

int get_first_free_index(ble_consumer_collection * p_ble_consumer_collection)
{
    int index = -1;
    esp_bd_addr_t zer_mac_address = {0};
    if (p_ble_consumer_collection != NULL)
    {
        if (p_ble_consumer_collection->consumers_count < p_ble_consumer_collection->size)
        {
            for (int i = 0; i < p_ble_consumer_collection->size; i++)
            {
                if (0 == memcmp(&(p_ble_consumer_collection->arr[i].mac_address_arr), zer_mac_address, sizeof(esp_bd_addr_t)))
                {
                    index = i;
                    break;
                }
            }
        }
    }
    return index;
}

int get_index_for_mac_addr(ble_consumer_collection * p_ble_consumer_collection, esp_bd_addr_t mac_address_arr)
{
    int index = -1;
    if (p_ble_consumer_collection != NULL && mac_address_arr != NULL)
    {
        for (int i = 0; i < p_ble_consumer_collection->size; i++)
        {
            if (0 == memcmp(&(p_ble_consumer_collection->arr[i].mac_address_arr), mac_address_arr, sizeof(esp_bd_addr_t)))
            {
                index = i;
                break;
            }
        }
    }

    return index;
}

ble_consumer * add_consumer_to_collection(ble_consumer_collection *p_collection, esp_bd_addr_t mac_address) {

    ble_consumer * p_ble_consumer = NULL;

    // Sprawdz parametry wejsciowe
    if (p_collection == NULL || mac_address == NULL) return p_ble_consumer;

    // Pobierz semafor
    if (xSemaphoreTake(p_collection->xMutex, portMAX_DELAY) != pdTRUE) {
        return p_ble_consumer;
    }

    // Pobierz pierwszy wolny indeks w tablicy z nadawcami
    int index = get_first_free_index(p_collection);
    if (index >= 0) {
        // Wyczyść strukturę nadawcy
        reset_ble_consumer(&p_collection->arr[index]);

        // Przekopiuj adres MAC nadawcy
        memcpy(p_collection->arr[index].mac_address_arr, mac_address, sizeof(esp_bd_addr_t));

        // Zapisz znacznik czasu ostatniego uzycia struktury nadawcy
        save_timestamp(&p_collection->arr[index].last_pdu_timestamp, &p_collection->arr[index].rollover);

        // Zwieksz liczbe aktywnych nadawcow
        p_collection->consumers_count++;

        p_ble_consumer = &(p_collection->arr[index]);
    } else {
        ESP_LOGE("BLE_COLLECTION", "No space available for new consumer!");
    }

    // Zwolnij Mutex
    xSemaphoreGive(p_collection->xMutex);
    return p_ble_consumer;
}

ble_consumer * get_ble_consumer_from_collection(ble_consumer_collection * p_ble_consumer_collection, esp_bd_addr_t mac_address_arr)
{
    ble_consumer * p_ble_consumer = NULL;

    // Sprawdz parametry wejsciowe
    if (p_ble_consumer_collection != NULL && mac_address_arr != NULL)
    {
        // Pobierz Mutex
        if (xSemaphoreTake(p_ble_consumer_collection->xMutex, portMAX_DELAY) == pdTRUE)
        {
            // Wyszukaj indeks nadawcy w tablicy na podstawie adresu MAC nadawcy
            int index = get_index_for_mac_addr(p_ble_consumer_collection, mac_address_arr);

            if (index >= 0)
            {
                // Zapisz wskaznik na strukture nadawcy
                p_ble_consumer = &(p_ble_consumer_collection->arr[index]);

                // Zapisz znacznik czasowy ostatniego uzycia
                save_timestamp(&(p_ble_consumer_collection->arr[index].last_pdu_timestamp), &(p_ble_consumer_collection->arr[index].rollover));
            }

            // Zwolnij Mutex
            xSemaphoreGive(p_ble_consumer_collection->xMutex);
        }
    }

    return p_ble_consumer;

}

int clear_ble_consumer_from_collection(ble_consumer_collection * p_ble_consumer_collection, const uint8_t index)
{
    int status = -1;
    if (p_ble_consumer_collection != NULL)
    {
        // Wyczyść strukturę nadawcy
        reset_ble_consumer(&(p_ble_consumer_collection->arr[index]));

        // Dekrementuj liczbe aktywnych nadawcow
        p_ble_consumer_collection->consumers_count--;
    }
    return status;
}

int remove_consumer_from_collection(ble_consumer_collection * p_ble_consumer_collection, esp_bd_addr_t mac_address_arr)
{
    int status = -1;
    // Sprawdz parametry wejsciowe
    if (p_ble_consumer_collection != NULL && mac_address_arr != NULL)
    {
        // Pobierz Mutex
        if (xSemaphoreTake(p_ble_consumer_collection->xMutex, portMAX_DELAY) == pdTRUE)
        {
            // Wyszukaj indeks nadawcy w tablicy na podstawie adresu MAC nadawcy
            int index = get_index_for_mac_addr(p_ble_consumer_collection, mac_address_arr);

            if (index >= 0)
            {
                // Wyczysc strukture nadawcy
                clear_ble_consumer_from_collection(p_ble_consumer_collection, index);
            }

            // Zwolnij Mutex
            xSemaphoreGive(p_ble_consumer_collection->xMutex);
        }
        status = 0;
    }
    return status;
}

int get_active_no_consumers(ble_consumer_collection * p_ble_consumer_collection)
{
    int active_consumers_count = -1;
    // Sprawdz parametry wejsciowe 
    if (p_ble_consumer_collection == NULL)
    {
        return active_consumers_count;
    }

    // Pobierz Mutex
    if (xSemaphoreTake(p_ble_consumer_collection->xMutex, portMAX_DELAY) == pdTRUE)
    {
        // Odczytaj liczbe aktywnych nadawcow
        active_consumers_count = p_ble_consumer_collection->consumers_count;

        // Zwolnij Mutex
        xSemaphoreGive(p_ble_consumer_collection->xMutex);
    }

    return active_consumers_count;
}


int remove_lru_consumer_from_collection(ble_consumer_collection * p_ble_consumer_collection)
{
    return -1;
}
