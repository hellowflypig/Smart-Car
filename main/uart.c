#include "MyApplication.h"
#include "uart.h"

/*
 * 初始化串口0
 */
esp_err_t uart0_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret;

    ret = uart_param_config(UART_NUM, &uart_config);
    if (ret != ESP_OK) return ret;

    ret = uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) return ret;

    ret = uart_driver_install(UART_NUM, UART_BUF_SIZE * 2, UART_BUF_SIZE * 2, 20, NULL, 0);
    if (ret != ESP_OK) return ret;

    return ESP_OK;
}

/* 向串口发送数据 */
esp_err_t uart0_send_data(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) {
        return ESP_FAIL;
    }

    int written = uart_write_bytes(UART_NUM, (const char *)data, len);
    return (written == (int)len) ? ESP_OK : ESP_FAIL;
}

/* 向串口发送字符串 */
esp_err_t uart0_send_string(const char *str)
{
    if (str == NULL) {
        return ESP_FAIL;
    }
    size_t len = strlen(str);
    int written = uart_write_bytes(UART_NUM, str, len);
    return (written == (int)len) ? ESP_OK : ESP_FAIL;
}

/* 从串口接收数据 */
int uart0_receive_data(uint8_t *data, size_t len, TickType_t ticks_to_wait)
{
    if (data == NULL || len == 0) {
        return -1;
    }

    return uart_read_bytes(UART_NUM, data, len, ticks_to_wait);
}

/* 检查串口是否有数据可读 */
bool uart0_data_available(void)
{
    size_t available_bytes = 0;
    uart_get_buffered_data_len(UART_NUM, &available_bytes);
    return (available_bytes > 0);
}

