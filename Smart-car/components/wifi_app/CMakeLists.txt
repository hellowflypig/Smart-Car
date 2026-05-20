idf_component_register(SRCS "wifi_app.c"
                    INCLUDE_DIRS "."
                    REQUIRES sys_config esp_wifi nvs_flash lwip camera motor servo)