idf_component_register(SRCS "esp_ibeacon_api.c" "ibeacon_demo.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_timer
                    REQUIRES nvs_flash
                    REQUIRES bt
                    PRIV_REQUIRES key_reconstruction
                    )
