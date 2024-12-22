idf_component_register(SRCS "ble_broadcast.c" "ble_sender_main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES core      
                    PRIV_REQUIRES esp_timer 
                    PRIV_REQUIRES ble_broadcast_sec_pdu_encryption_engine     
                    )
