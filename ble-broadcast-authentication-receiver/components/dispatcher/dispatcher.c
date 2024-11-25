#include "dispatcher.h"
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char* DIST_LOG_GROUP = "DISTRIBUATION TASK";
static const char* DISTRIBUATION_TASK_NAME = "DISPATCHER TASK";
TaskHandle_t *xDistribuationTask = NULL;

void PassPduToDistribuation(void)
{

}

void SpawnDistribuatorTask(void) {
    if (xDistribuationTask == NULL)
    {
        BaseType_t  taskCreateResult = xTaskCreatePinnedToCore(
            DispatcherTaskMain,
            DISTRIBUATION_TASK_NAME, 
            (uint32_t) DISPATCHER_TASK_SIZE,
            NULL,
            (UBaseType_t) DISPATCHER_TASK_PRIORITY,
            xDistribuationTask,
            DISPATCHER_TASK_CORE
            );
    
        if (taskCreateResult != pdPASS)
        {
            ESP_LOGE(DIST_LOG_GROUP, "Task was not created successfully! :(");
        }
        else
        {
            ESP_LOGI(DIST_LOG_GROUP, "Task was created successfully! :)");
        }
    
    }

}


void DispatcherTaskMain(void *arg)
{
    const TickType_t delayMs = 500 / portTICK_PERIOD_MS;
    while (1)
    {
        PassPduToDistribuation();
        ESP_LOGI(DIST_LOG_GROUP, "Hello from distribuation task :)");
        vTaskDelay(delayMs);
    }
}







