idf_component_register(SRCS "pca9535.c"
                    INCLUDE_DIRS "."
                    REQUIRES sys_config driver bsp_i2c)
