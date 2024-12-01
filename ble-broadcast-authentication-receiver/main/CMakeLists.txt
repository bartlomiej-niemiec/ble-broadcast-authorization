idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_timer
                    REQUIRES nvs_flash
                    REQUIRES bt
                    PRIV_REQUIRES ble_gap
                    )
