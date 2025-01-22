idf_component_register(
        SRCS "./src/pc_serial_communication.c"
        INCLUDE_DIRS "./include"
        PRIV_REQUIRES esp_driver_uart
        PRIV_REQUIRES "utils"
)
