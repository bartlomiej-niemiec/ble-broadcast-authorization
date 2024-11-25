idf_component_register(SRCS "ble_gap.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash
                    REQUIRES bt
                    )
