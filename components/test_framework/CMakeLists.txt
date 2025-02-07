idf_component_register(SRCS "./src/test.c"
                    INCLUDE_DIRS
                            "./include"
                    REQUIRES esp_timer
                    REQUIRES bt
                    REQUIRES core
                )
