#ifndef UART_H
#define UART_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include "freertos/semphr.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// UART 配置宏
#define UART_NUM UART_NUM_0
#define UART_BAUD_RATE 921600
#define UART_DATA_8_BITS UART_DATA_8_BITS
#define UART_PARITY_DISABLE UART_PARITY_DISABLE
#define UART_STOP_BITS_1 UART_STOP_BITS_1
#define UART_HW_FLOWCTRL_DISABLE UART_HW_FLOWCTRL_DISABLE
#define UART_SCLK_DEFAULT UART_SCLK_DEFAULT
#define UART_TX_PIN GPIO_NUM_43  // ESP32S3 UART0 TX
#define UART_RX_PIN GPIO_NUM_44  // ESP32S3 UART0 RX
#define UART_BUF_SIZE 1024

// 函数声明
esp_err_t uart0_init(void);
esp_err_t uart0_send_data(const uint8_t *data, size_t len);
esp_err_t uart0_send_string(const char *str);
int uart0_receive_data(uint8_t *data, size_t len, TickType_t ticks_to_wait);
bool uart0_data_available(void);

#endif // UART_H