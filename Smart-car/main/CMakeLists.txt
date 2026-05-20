idf_component_register(SRCS
                    INCLUDE_DIRS "."
                    REQUIRES sys_config bsp_i2c pca9535 motor servo oled camera wifi_app esp_timer)
