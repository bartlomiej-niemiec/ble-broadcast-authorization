idf_component_register(SRCS "crypto.c"
                INCLUDE_DIRS "."
                PRIV_REQUIRES mbedtls
                PRIV_REQUIRES esp_system
        )
