#include <stddef.h>

#include "key_management.h"
#include "key_reconstructor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char* RECONSTRUCTION_TASK_NAME = "KEY_RECONSTRUCTION_TASK";
static const char* REC_LOG_GROUP = "RECONSTRUCTION TASK";
key_management key_collections[KEY_COLLECTIONS_SIZE];
TaskHandle_t *xRecontructionKeyTask = NULL;

void ReconstructorTaskMain(void *arg);

void SpawnRecontructionKeyTask(void) {
    if (xRecontructionKeyTask == NULL)
    {
        BaseType_t  taskCreateResult = xTaskCreatePinnedToCore(
            ReconstructorTaskMain,
            RECONSTRUCTION_TASK_NAME, 
            (uint32_t) KEY_RECONSTRUCTION_TASK_SIZE,
            NULL,
            (UBaseType_t) RECONSTRUCTOR_TASK_PRIORITY,
            xRecontructionKeyTask,
            RECONSTRUCTOR_TASK_CORE
            );
    
        if (taskCreateResult != pdPASS)
        {
            ESP_LOGE(REC_LOG_GROUP, "Task was not created successfully! :(");
        }
        else
        {
            ESP_LOGI(REC_LOG_GROUP, "Task was created successfully! :)");
        }
    
    }

}


void ReconstructorTaskMain(void *arg)
{
    const TickType_t delayMs = 500 / portTICK_PERIOD_MS;
    while (1)
    {
        ESP_LOGI(REC_LOG_GROUP, "Hello from reconstruction task :)");
        vTaskDelay(delayMs);
    }
}







