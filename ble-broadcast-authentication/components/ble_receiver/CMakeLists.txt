idf_component_register(SRCS "key_cache.c" "sec_pdu_processing.c" "dispatcher.c" "key_reconstruction/key_reconstructor.c" "key_reconstruction/key_management.c" "ble_receiver.c"
                    INCLUDE_DIRS "." "./key_reconstruction"
                    REQUIRES ble_gap
                    PRIV_REQUIRES esp_ringbuf
                    PRIV_REQUIRES beacon_pdu
                    PRIV_REQUIRES esp_timer
                    PRIV_REQUIRES crypto
                    )
