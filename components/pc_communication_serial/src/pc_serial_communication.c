#include "pc_serial_communication.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>

#include "tasks_data.h"

#define UART_PORT_NUM                       UART_NUM_0
#define UART_BAUD_RATE                      115200
#define BUF_SIZE                            1024
#define QUEUE_SIZE                          10
#define PC_SERIAL_TASK_DELAY_MS             100
#define PC_SERIAL_TASK_DELAY_SYS_TICK       pdMS_TO_TICKS(PC_SERIAL_TASK_DELAY_MS)
#define QUEUE_TIMEOUT_SYS_TICK              pdMS_TO_TICKS(PC_SERIAL_TASK_DELAY_MS)
#define PC_SERIAL_LOG_GROUP                 "PC_SERIAL_COMM"

typedef struct {
    QueueHandle_t uart_event_queue;
    const uart_port_t uart_num;
    serial_data_received_cb cb;
    TaskHandle_t xTask;
} pc_serial_control;

static pc_serial_control control_st = {
    .uart_num = UART_NUM_0,
    .cb =  NULL,
    .xTask = NULL,
    .uart_event_queue = NULL,
};

void pc_serial_main(void *arg)
{
    uart_event_t event;
    uint8_t data[BUF_SIZE] = {0};
    while (true) {
        // Wait for UART event.
        if (xQueueReceive(control_st.uart_event_queue, (void * )&event, QUEUE_TIMEOUT_SYS_TICK)) {
            switch (event.type) {
                // Event of UART receiving data
                case UART_DATA:
                    if (control_st.cb != NULL)
                    {
                        uart_read_bytes(control_st.uart_num, data, event.size, portMAX_DELAY);
                        control_st.cb(data, event.size);
                    }
                    break;

                case UART_FIFO_OVF:
                    uart_flush_input(control_st.uart_num);
                    break;

                case UART_BUFFER_FULL:
                    uart_flush_input(control_st.uart_num);
                    break;

                default:
                    break;
            }
        }
    }
}


void register_serial_data_received_cb(serial_data_received_cb cb)
{
    control_st.cb = cb;
}

void send_test_string_to_pc()
{
    // Write data to UART.
    char* test_str = "This is a test string.\n";
    uart_write_bytes(control_st.uart_num, (const char*)test_str, strlen(test_str));
}


esp_err_t start_pc_serial_communication()
{
    uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_param_config(control_st.uart_num, &uart_config);
    esp_err_t result =  uart_driver_install(control_st.uart_num, BUF_SIZE, BUF_SIZE, QUEUE_SIZE, &control_st.uart_event_queue, 0);

    if (result == ESP_OK)
    {
        (void) xTaskCreate(
            pc_serial_main,
            tasksDataArr[PC_SERIAL_COMMUNICATION_TASK].name,
            tasksDataArr[PC_SERIAL_COMMUNICATION_TASK].stackSize,
            NULL,
            tasksDataArr[PC_SERIAL_COMMUNICATION_TASK].priority,
            &control_st.xTask
        );

        if (control_st.xTask == NULL)
        {
            ESP_LOGE(PC_SERIAL_LOG_GROUP, "Failed to startup task");
            result = ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGE(PC_SERIAL_LOG_GROUP, "Failed to initialized uart driver");
    }

    return result;
}