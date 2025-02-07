idf_component_register(
        SRCS "./src/ble_security_payload_encryption.c"
        INCLUDE_DIRS "./include"
        PRIV_REQUIRES "core" "esp_timer" "test_framework"
)
