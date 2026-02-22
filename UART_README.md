
# ESP32-S3 串口0通信使用说明

## 概述

本项目已添加ESP32-S3串口0(UART0)的通信功能，可用于与上位机进行数据交互和调试信息打印。

## 文件说明

- `uart.h`: 串口通信头文件，定义了串口通信相关的API
- `uart.c`: 串口通信实现文件
- `uart_example.c`: 串口通信示例代码，展示如何使用串口API

## 串口配置

串口0默认配置如下：
- 波特率: 115200
- 数据位: 8位
- 停止位: 1位
- 校验位: 无
- 流控: 无
- TX引脚: GPIO1
- RX引脚: GPIO3

## API函数说明

### 1. 初始化串口
```c
esp_err_t uart0_init(void);
```
初始化串口0，配置通信参数。需要在程序开始时调用一次。

### 2. 发送数据
```c
esp_err_t uart0_send_data(const uint8_t *data, size_t len);
```
向串口发送指定长度的二进制数据。

### 3. 发送字符串
```c
esp_err_t uart0_send_string(const char *str);
```
向串口发送以null结尾的字符串。

### 4. 接收数据
```c
int uart0_receive_data(uint8_t *data, size_t len, TickType_t ticks_to_wait);
```
从串口接收数据，最多读取len字节。
参数:
- data: 存储接收数据的缓冲区
- len: 缓冲区大小
- ticks_to_wait: 等待超时时间(单位: FreeRTOS tick)
返回值: 实际接收到的字节数，超时返回0，错误返回-1

### 5. 检查数据可用性
```c
bool uart0_data_available(void);
```
检查串口是否有数据可读。返回true表示有数据可读。

### 6. 格式化打印
```c
void uart0_printf(const char *format, ...);
```
类似于标准库的printf函数，格式化输出字符串到串口。

## 使用示例

### 基本初始化
```c
#include "uart.h"

void app_main(void)
{
    // 初始化串口
    uart0_init();

    // 打印启动信息
    uart0_printf("ESP32-S3 串口初始化完成\n");

    // 发送字符串
    uart0_send_string("Hello from ESP32-S3!\n");

    // 发送二进制数据
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uart0_send_data(data, sizeof(data));
}
```

### 接收数据示例
```c
void receive_task(void *pvParameters)
{
    uint8_t rx_buffer[128];

    while (1) {
        if (uart0_data_available()) {
            int len = uart0_receive_data(rx_buffer, sizeof(rx_buffer), 100 / portTICK_PERIOD_MS);

            if (len > 0) {
                // 处理接收到的数据
                uart0_printf("收到 %d 字节数据\n", len);
                // 回显数据
                uart0_send_data(rx_buffer, len);
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
```

## 上位机连接

1. 使用USB数据线连接ESP32-S3开发板到电脑
2. 打开串口终端软件(如PuTTY、SecureCRT或Arduino IDE串口监视器)
3. 设置串口参数:
   - 波特率: 115200
   - 数据位: 8
   - 停止位: 1
   - 校验位: 无
   - 流控: 无
4. 打开串口，即可与ESP32-S3进行通信

## 注意事项

1. ESP32-S3的串口0默认使用GPIO1(TX)和GPIO3(RX)，这些引脚可能与其他外设冲突，请确保没有其他外设使用这些引脚。
2. 串口0也用于ESP32的启动日志和调试输出，因此可能会看到一些系统启动信息。
3. 在使用串口通信时，确保上位机和ESP32-S3的串口参数一致，否则可能导致乱码。
4. 接收数据时，建议使用任务循环检查数据，而不是阻塞等待，以提高系统响应性。
