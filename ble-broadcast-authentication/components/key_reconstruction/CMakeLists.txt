idf_component_register(SRCS "key_management.c"
                            "key_reconstructor.c"
                INCLUDE_DIRS "."
                PRIV_REQUIRES crypto
        )
