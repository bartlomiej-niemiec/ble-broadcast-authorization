idf_component_register(SRCS "src/ble_consumer/ble_consumer_collection.c"
                            "src/ble_consumer/ble_consumer.c"
                            "src/key_cache/key_cache.c"
                            "src/key_reconstruction/key_reconstructor.c"
                            "src/key_reconstruction/key_management.c"
                            "src/sec_payload_observer_collection.c"
                            "src/sec_pdu_processing.c"
                    INCLUDE_DIRS "./include"
                    PRIV_INCLUDE_DIRS "./internal"
                                      "./internal/ble_consumer"
                                      "./internal/key_cache"
                                      "./internal/key_reconstruction"
                    PRIV_REQUIRES core     
                    PRIV_REQUIRES utils
                    PRIV_REQUIRES "ble_broadcast_controller"
                    )
