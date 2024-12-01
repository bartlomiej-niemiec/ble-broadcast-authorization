idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES ble_gap
                    PRIV_REQUIRES ble_sender
                    PRIV_REQUIRES ble_receiver
                    )
