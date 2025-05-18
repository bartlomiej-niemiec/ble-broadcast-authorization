idf_component_register(SRCS "./src/beacon_pdu/beacon_pdu_data.c"
                            "./src/beacon_pdu/beacon_test_pdu.c"
                            "./src/crypto/crypto.c"
                            "./src/ble_common/ble_common.c"
                    INCLUDE_DIRS
                            "./include"
                            "./include/beacon_pdu"
                            "./include/ble_common"
                            "./include/crypto"
                            "./include/config"
                    REQUIRES esp_timer
                    REQUIRES nvs_flash
                    REQUIRES bt
                    REQUIRES mbedtls
                    REQUIRES esp_system
                )
