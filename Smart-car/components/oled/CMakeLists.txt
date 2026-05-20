idf_component_register(SRCS "oled.c"
                    INCLUDE_DIRS "."
                    REQUIRES sys_config driver bsp_i2c)