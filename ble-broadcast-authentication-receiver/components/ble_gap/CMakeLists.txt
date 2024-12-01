idf_component_register(SRCS "ble_common.c" "ble_receiver.c" "ble_sender.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash
                    REQUIRES bt
                    PRIV_REQUIRES dispatcher
                    PRIV_REQUIRES beacon_pdu
                    PRIV_REQUIRES esp_timer
                    )
