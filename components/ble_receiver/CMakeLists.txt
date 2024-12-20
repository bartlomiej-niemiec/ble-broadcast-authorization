idf_component_register(SRCS "ble_consumer/ble_consumer_collection.c"
                            "ble_consumer/ble_consumer.c"
                            "key_cache/key_cache.c"
                            "key_reconstruction/key_reconstructor.c"
                            "key_reconstruction/key_management.c"
                            "utilities/tick_count_timestamp.c"
                            "sec_pdu_processing.c"
                            "dispatcher.c"
                            "ble_receiver.c"
                    INCLUDE_DIRS "." "./key_reconstruction" "./ble_consumer" "./key_cache" "./utilities"
                    REQUIRES ble_gap
                    PRIV_REQUIRES esp_ringbuf
                    PRIV_REQUIRES beacon_pdu
                    PRIV_REQUIRES esp_timer
                    PRIV_REQUIRES crypto
                    )
