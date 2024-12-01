idf_component_register(SRCS "common/ble_common.c"
                    INCLUDE_DIRS "./common"
                    REQUIRES nvs_flash
                    REQUIRES bt
                    )