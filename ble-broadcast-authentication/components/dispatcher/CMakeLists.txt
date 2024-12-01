idf_component_register(SRCS "dispatcher.c"
                INCLUDE_DIRS "."
                PRIV_REQUIRES beacon_pdu
                PRIV_REQUIRES esp_ringbuf
        )
