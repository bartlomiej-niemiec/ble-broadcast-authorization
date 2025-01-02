idf_component_register(
        SRCS "./src/ble_broadcast_security_transmitter_engine.c"
        INCLUDE_DIRS "./include"
        PRIV_INCLUDE_DIRS "./src"
        PRIV_REQUIRES "core" "esp_timer" "ble_broadcast_controller"
)
