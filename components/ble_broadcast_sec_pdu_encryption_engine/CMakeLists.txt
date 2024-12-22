idf_component_register(SRCS "src/ble_msg_encryptor.c"
                    INCLUDE_DIRS "./include"
                    PRIV_REQUIRES core      
                    PRIV_REQUIRES esp_timer      
                    )
