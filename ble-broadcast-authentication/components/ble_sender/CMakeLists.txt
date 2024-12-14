idf_component_register(SRCS "ble_msg_encryptor.c" "ble_sender.c" "ble_broadcast.c"
                    INCLUDE_DIRS "."
                    REQUIRES ble_gap
                    PRIV_REQUIRES beacon_pdu
                    PRIV_REQUIRES esp_timer
                    REQUIRES crypto
                    )
