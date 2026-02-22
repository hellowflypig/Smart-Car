idf_component_register(SRCS "encoder.c" "uart.c" "motor_driver.c" "ov2640.c" "lcd.c" "led_pwm.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp32-camera esp_timer driver freertos log console)
