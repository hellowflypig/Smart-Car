idf_component_register(SRCS "motor.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_timer sys_config driver pca9535)
