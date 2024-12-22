idf_component_register(
    SRCS "./main.c" "./ble_gap_payload_consuming.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        ble_broadcast_security_processing_engine
        core
        utils
)
