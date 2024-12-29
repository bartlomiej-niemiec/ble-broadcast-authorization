idf_component_register(
        SRCS "./src/ble_broadcast_controller.c"
        INCLUDE_DIRS "./include"
        PRIV_REQUIRES "core" "esp_timer"
)
