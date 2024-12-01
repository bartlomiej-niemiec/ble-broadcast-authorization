idf_component_register(SRCS "ble_receiver.c"
                    INCLUDE_DIRS "."
                    REQUIRES ble_gap
                    PRIV_REQUIRES dispatcher
                    PRIV_REQUIRES beacon_pdu
                    PRIV_REQUIRES esp_timer
                    )
