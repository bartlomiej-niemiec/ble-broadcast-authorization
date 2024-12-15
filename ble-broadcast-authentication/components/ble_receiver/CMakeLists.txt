idf_component_register(SRCS "key_cache.c" "pdu_decryptor.c" "dispatcher.c" "key_reconstructor.c" "key_management.c" "ble_receiver.c"
                    INCLUDE_DIRS "."
                    REQUIRES ble_gap
                    PRIV_REQUIRES esp_ringbuf
                    PRIV_REQUIRES beacon_pdu
                    PRIV_REQUIRES esp_timer
                    PRIV_REQUIRES crypto
                    )
