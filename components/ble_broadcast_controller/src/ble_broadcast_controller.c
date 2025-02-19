#include "ble_broadcast_controller.h"
#include "beacon_pdu_data.h"
#include "ble_common.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"

#include "esp_log.h"
#include "esp_gap_ble_api.h"
#include "esp_timer.h"
#include "esp_random.h"
#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>

#include "tasks_data.h"

#define EVENT_QUEUE_SIZE 10
#define MINUS_INTERVAL_TOLERANCE_MS 10
#define PLUS_INTERVAL_TOLERANCE_MS 10

#define SEMAPHORE_TIMEOUT_MS 20
#define QUEUE_TIMEOUT_MS 20

#define N_CONST 0.625
#define MS_TO_N_CONVERTION(MS) ((uint16_t)((MS) / (N_CONST)))

#define ADV_INT_MIN_MS 100
#define ADV_INT_MAX_MS 110

#define MAX_SCAN_COMPLETE_CB 2

#define EVENT_QUEUE_TIMEOUT_MS 10
#define EVENT_QUEUE_TIMEOUT_SYSTICK pdMS_TO_TICKS(EVENT_QUEUE_TIMEOUT_MS)

static const char* BROADCAST_LOG_GROUP = "BROADCAST TASK";

#define BLE_ADV_DATA_RAW_SET_COMPLETE_EVT (1 << 0)
#define BLE_ADV_START_COMPLETE_EVT (1 << 1)
#define BLE_ADV_STOP_COMPLETE_EVT (1 << 2)
#define BLE_SCAN_START_COMPLETE_EVT (1 << 3)
#define BLE_SCAN_STOP_COMPLETE_EVT (1 << 4)
#define BLE_SCAN_RESULT_EVT (1 << 5)

typedef struct {
    EventGroupHandle_t eventGroup;
    atomic_int   broadcastState;
    atomic_int   scannerState;
    TaskHandle_t xBroadcastTaskHandle;
    esp_ble_adv_params_t ble_adv_params;
    broadcast_state_changed_callback state_change_cb;
    broadcast_new_data_set_cb data_set_cb;
    scan_complete scan_complete_cb[MAX_SCAN_COMPLETE_CB];
    int scan_complete_cb_observers;
    SemaphoreHandle_t xMutex;
} broadcast_control_structure;


static broadcast_control_structure bc = 
{
    .broadcastState = BROADCAST_CONTROLLER_BROADCASTING_NOT_RUNNING,
    .scannerState = SCANNER_CONTROLLER_SCANNING_NOT_ACTIVE,
    .state_change_cb = NULL,
    .data_set_cb = NULL,
    .scan_complete_cb = {NULL, NULL},
    .scan_complete_cb_observers = 0
};

void set_broadcast_state(BroadcastState state);
void set_scanner_state(ScannerState state);


static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {    
    int64_t timestamp;
    // // Handle specific event types
    switch (event)
    {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        {
            xEventGroupSetBits(bc.eventGroup, BLE_ADV_DATA_RAW_SET_COMPLETE_EVT);
        }
        break;

        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        {
            xEventGroupSetBits(bc.eventGroup, BLE_ADV_START_COMPLETE_EVT);
        }
        break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        {
            xEventGroupSetBits(bc.eventGroup, BLE_ADV_STOP_COMPLETE_EVT);
        }
        break;

        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        {
            xEventGroupSetBits(bc.eventGroup, BLE_SCAN_START_COMPLETE_EVT);
        }
        break;

        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        {
            xEventGroupSetBits(bc.eventGroup, BLE_SCAN_STOP_COMPLETE_EVT);
        }
        break;

        case ESP_GAP_BLE_SCAN_RESULT_EVT:
        {
            // Validate the parameter
            if (param == NULL) {
                ESP_LOGE(BROADCAST_LOG_GROUP, "Callback parameter is NULL");
                return;
            }
            esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
            if (scan_result->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                timestamp = esp_timer_get_time();
                // Safely handle payload size
                if (scan_result->scan_rst.adv_data_len <= MAX_GAP_DATA_LEN) {
                    if (bc.scan_complete_cb_observers > 0)
                    {
                        for (int j = 0; j < bc.scan_complete_cb_observers; j++)
                        {
                            bc.scan_complete_cb[j](timestamp, scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len, scan_result->scan_rst.bda);
                        }
                    }
                } else {
                    ESP_LOGW(BROADCAST_LOG_GROUP, "Adv data too large: %d bytes", scan_result->scan_rst.adv_data_len);
                }
            }
        }
        break;

        default:
            break;

    };
}

static void ble_appRegister(void)
{
    esp_err_t status;

    ESP_LOGI(BROADCAST_LOG_GROUP, "register callback");

    //register the scan callback function to the gap module
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK)
    {
        ESP_LOGE(BROADCAST_LOG_GROUP, "gap register error: %s", esp_err_to_name(status));
        return;
    }
}


static esp_err_t init_resources()
{
    if (bc.xMutex == NULL) {
        bc.xMutex = xSemaphoreCreateMutex();
        if (bc.xMutex == NULL) {
            ESP_LOGE(BROADCAST_LOG_GROUP, "Failed to create mutex!");
            return ESP_ERR_NO_MEM;
        }
    }

    bc.eventGroup = xEventGroupCreate();

    if (bc.eventGroup == NULL) {
        ESP_LOGE(BROADCAST_LOG_GROUP, "Failed to create event group!");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGE(BROADCAST_LOG_GROUP, "Initialization succed!");

    return ESP_OK;
}


static void ble_sender_main(void) {
    while (1) {

        // Pętla zdarzeń - pobierz nowe zdarzenie
        EventBits_t events = xEventGroupWaitBits(bc.eventGroup,
                                                 BLE_ADV_DATA_RAW_SET_COMPLETE_EVT |
                                                 BLE_ADV_START_COMPLETE_EVT |
                                                 BLE_ADV_STOP_COMPLETE_EVT |
                                                 BLE_SCAN_START_COMPLETE_EVT |
                                                 BLE_SCAN_STOP_COMPLETE_EVT,
                                                 pdTRUE, pdFALSE, portMAX_DELAY);

       
            // Obsłuż zdarzenie nowego ustawienia danych do rozgłaszania
            if (events & BLE_ADV_DATA_RAW_SET_COMPLETE_EVT)
            {
                if (bc.data_set_cb) {
                    // Zawołaj funkcję zwrotną klienta
                    bc.data_set_cb();
                }
            }

            // Obsłuż zdarzenie start transmisji rozgłoszeniowej
            if (events & BLE_ADV_START_COMPLETE_EVT)
            {
                // Ustaw stan transmisji na aktywny
                set_broadcast_state(BROADCAST_CONTROLLER_BROADCASTING_RUNNING);
                if (bc.state_change_cb) {
                    // Zawołaj funkcję zwrotną klienta
                    bc.state_change_cb(BROADCAST_CONTROLLER_BROADCASTING_RUNNING);
                }
                ESP_LOGI(BROADCAST_LOG_GROUP, "Broadcast started!");
            }

            // Obsłuż zdarzenie stop transmisji rozgłoszeniowej
            if (events & BLE_ADV_STOP_COMPLETE_EVT)
            {
                // Ustaw stan transmisji na nieaktywny
                set_broadcast_state(BROADCAST_CONTROLLER_BROADCASTING_NOT_RUNNING);
                if (bc.state_change_cb) {
                    // Zawołaj funkcję zwrotną klienta
                    bc.state_change_cb(BROADCAST_CONTROLLER_BROADCASTING_NOT_RUNNING);
                }
                ESP_LOGI(BROADCAST_LOG_GROUP, "Broadcast stopped!");
            }

            // Obsłuż zdarzenie start skanowania pakietów rozgłoszeniowych
            if (events & BLE_SCAN_START_COMPLETE_EVT)
            {
                // Ustaw stan skanowania na aktywny
                set_scanner_state(SCANNER_CONTROLLER_SCANNING_ACTIVE);
            }

            // Obsłuż zdarzenie stop skanowania pakietów rozgłoszeniowych
            if (events & BLE_SCAN_STOP_COMPLETE_EVT)
            {
                // Ustaw stan skanowania na nieaktywny
                set_scanner_state(SCANNER_CONTROLLER_SCANNING_NOT_ACTIVE);
            }
    }
}


bool init_broadcasting()
{
    static bool ble_controller_initialized = false;
    if (ble_controller_initialized == false)
    {
        ble_controller_initialized = init_resources() == ESP_OK ? true: false;
    }
    return ble_controller_initialized;
}

bool init_broadcast_controller()
{
    
    bool status = init_broadcasting();
    
    if (status == true)
    {
        if (xSemaphoreTake(bc.xMutex, pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
        {
            ble_init();
            ble_appRegister();
            if (bc.xBroadcastTaskHandle == NULL)
            {
                BaseType_t  taskCreateResult = xTaskCreatePinnedToCore(
                    (TaskFunction_t) ble_sender_main,
                    tasksDataArr[BLE_CONTROLLER_TASK].name, 
                    tasksDataArr[BLE_CONTROLLER_TASK].stackSize,
                    NULL,
                    tasksDataArr[BLE_CONTROLLER_TASK].priority,
                    (TaskHandle_t * )&(bc.xBroadcastTaskHandle),
                    tasksDataArr[BLE_CONTROLLER_TASK].core
                    );
            
                if (taskCreateResult != pdPASS)
                {
                    bc.xBroadcastTaskHandle = NULL;
                    ESP_LOGE(BROADCAST_LOG_GROUP, "Task was not created successfully! :(");
                    return false;
                }
                else
                {
                    ESP_LOGI(BROADCAST_LOG_GROUP, "Task was created successfully! :)");
                }
            
            }
            else
            {
                ESP_LOGW(BROADCAST_LOG_GROUP, "Task already running!");
            }

            xSemaphoreGive(bc.xMutex);
        }
    }

    return true;
}

void register_broadcast_state_change_callback(broadcast_state_changed_callback cb)
{
    if (xSemaphoreTake(bc.xMutex, pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
    {
        bc.state_change_cb = cb;
        xSemaphoreGive(bc.xMutex);
    }
}

void register_broadcast_new_data_callback(broadcast_new_data_set_cb cb)
{
    if (xSemaphoreTake(bc.xMutex, pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
    {
        bc.data_set_cb = cb;
        xSemaphoreGive(bc.xMutex);
    }
}

void register_scan_complete_callback(scan_complete cb)
{
    if (xSemaphoreTake(bc.xMutex, pdMS_TO_TICKS(SEMAPHORE_TIMEOUT_MS)) == pdTRUE)
    {
        if (bc.scan_complete_cb_observers < 2)
        {
            bc.scan_complete_cb[bc.scan_complete_cb_observers] = cb;
            bc.scan_complete_cb_observers++;
        }
        xSemaphoreGive(bc.xMutex);
    }
}

// Ustaw nowe dane do rozgłaszania
void set_broadcasting_payload(uint8_t *payload, size_t payload_size)
{
    // Sprawdz bufor danych i stan transmisji
    if (payload_size <= MAX_GAP_DATA_LEN && payload != NULL &&
        get_broadcast_state() == BROADCAST_CONTROLLER_BROADCASTING_RUNNING)
    {
        // Zgłoś żądanie nowych danych do stosu BLE
        esp_ble_gap_config_adv_data_raw(payload, payload_size);
    } 
}

// Stop transmisji rozgłoszeniowej
void stop_broadcasting()
{
    // Sprawdz czy transmisja pakietów rozgłoszeniowych aktywna
    if (get_broadcast_state() == BROADCAST_CONTROLLER_BROADCASTING_RUNNING)
    {
        // Zgłoś żądanie stop transmisji do stosu BLE
        esp_ble_gap_stop_advertising();
    }
}

// Start transmisji rozgłoszeniowej z podanymi parametrami
void start_broadcasting(esp_ble_adv_params_t * ble_adv_params) {
    // Sprawdz czy urzadzenie jest w trybie skanowania
    if (get_scanner_state() == SCANNER_CONTROLLER_SCANNING_ACTIVE) {
        ESP_LOGE(BROADCAST_LOG_GROUP, "Stopping Broadcasting");
        // zakoncz skanowanie
        stop_scanning();
    }
    // Sprawdz czy transmisja pakietów rozgłoszeniowych nieaktywna
    if (get_broadcast_state() == BROADCAST_CONTROLLER_BROADCASTING_NOT_RUNNING) {
        ESP_LOGE(BROADCAST_LOG_GROUP, "Starting Broadcasting");
        // Ustaw moc nadajnika na maksymalna
        esp_err_t result = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
        result &= esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
        if (result != ESP_OK)
        {
            ESP_LOGE(BROADCAST_LOG_GROUP, "Error while setting tx power!");
        }
        // Zgłoś żądanie startu do stosu BLE
        esp_ble_gap_start_advertising(ble_adv_params);
    }
}

// Ustaw stan transmisji rozgłoszeniowej
void set_broadcast_state(BroadcastState state)
{
    atomic_store(&bc.broadcastState, state);
}

//Pobierz stan transmisji rozgłoszeniowej
BroadcastState get_broadcast_state()
{
    return atomic_load(&bc.broadcastState);
}

// Start skanowania pakietów rozgłoszeniowych według podanych parametrów
void start_scanning(esp_ble_scan_params_t scan_params, uint32_t scan_duration_s) {
    // Sprawdz czy urzadzenie jest w trybie rozgłaszania 
    if (get_broadcast_state() == BROADCAST_CONTROLLER_BROADCASTING_RUNNING) {
        // zakoncz rozgłąszanie
        stop_broadcasting();
    }
    // Sprawdz czy skanowanie pakietów rozgłoszeniowych nieaktywne
    if (get_scanner_state() == SCANNER_CONTROLLER_SCANNING_NOT_ACTIVE) {
        // Ustaw parametry skanowania
        esp_ble_gap_set_scan_params(&scan_params);
        // Rozpocznij skanowanie
        esp_ble_gap_start_scanning(scan_duration_s);
    }
}


void stop_scanning()
{
    // Sprawdz czy skanowanie pakietów rozgłoszeniowych aktywne
    if (get_scanner_state() == SCANNER_CONTROLLER_SCANNING_ACTIVE)
    {
        // Zakoncz skanowanie
        esp_ble_gap_stop_scanning();
    }
}

// Ustaw stan skanowania
ScannerState get_scanner_state()
{
    return atomic_load(&bc.scannerState);
}

//Pobierz stan skanowania
void set_scanner_state(ScannerState state)
{
    atomic_store(&bc.scannerState, state);
}