/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "MyApplication.h"
#include "lcd.h"

/* Private define-------------------------------------------------------------*/
#define SPI_HOST SPI2_HOST  // 使用SPI2
#define DMA_CHAN 0          // DMA通道自动分配（ESP32-S3要求）
#define TAG "TFT"

/* Private variables----------------------------------------------------------*/
static spi_device_handle_t spi_handle;

/* Private function prototypes------------------------------------------------*/
static void tft_init();
static void tft_set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
static void tft_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
static void tft_fill_screen(uint16_t color);
static void tft_draw_rectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
static void tft_backlight_on();
static void tft_backlight_off();

// 辅助函数
static void spi_write_command(uint8_t cmd);
static void spi_write_data(uint8_t data);
static void spi_write_data16(uint16_t data);
static void st7735s_init_sequence();

/* Public variables-----------------------------------------------------------*/
TFT_t TFT = 
{
    tft_init,
    tft_set_address_window,
    tft_draw_pixel,
    tft_fill_screen,
    tft_draw_rectangle,
    tft_backlight_on,
    tft_backlight_off
};

/* Private functions ---------------------------------------------------------*/
/**
 * @brief TFT显示屏初始化函数
 * 该函数用于初始化TFT显示屏，包括GPIO配置、SPI总线初始化、SPI设备配置、硬件复位和显示屏初始化
 */
static void tft_init() 
{
    // 配置GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TFT_CS) | (1ULL << TFT_DC) | (1ULL << TFT_RST) | (1ULL << TFT_BLK),
        .mode = GPIO_MODE_OUTPUT,    // 输出模式
        .pull_up_en = GPIO_PULLUP_DISABLE,    // 不使用上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,      // 不使用下拉
        .intr_type = GPIO_INTR_DISABLE  // 不使用中断
    };
    gpio_config(&io_conf);

    // 初始化SPI总线
    spi_bus_config_t buscfg = {
        .mosi_io_num = TFT_SDA, // MOSI引脚
        .miso_io_num = -1,  // 不使用MISO
        .sclk_io_num = TFT_SCL,  // SCLK引脚
        .quadwp_io_num = -1,    // 不使用WP
        .quadhd_io_num = -1,    // 不使用HD
        .max_transfer_sz = 4096  // 单次SPI传输的最大数据大小，单位是字节，设置为4096字节，避免DMA限制
    };
    esp_err_t ret = spi_bus_initialize(SPI_HOST, &buscfg, DMA_CHAN); //ESP32-S3 不支持手动指定 DMA 通道，必须使用自动分配的DMA通道   
    if (ret != ESP_OK) {
        // printf("%s: SPI bus init failed: %s\n", TAG, esp_err_to_name(ret));
        return;
    }
    // printf("%s: SPI bus initialized with DMA auto-allocation\n", TAG);

    // 配置SPI设备
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,  //40MHz
        .mode = 0,  // SPI模式0：CPOL=0, CPHA=0
        .spics_io_num = TFT_CS,  // CS引脚  
        .queue_size = 7,  // 队列大小，即可以同时发送的命令数量
        .pre_cb = NULL, // 命令发送前的回调函数
        .post_cb = NULL // 命令发送后的回调函数
    };
    ret = spi_bus_add_device(SPI_HOST, &devcfg, &spi_handle); // 添加SPI设备到总线
    if (ret != ESP_OK) {
        // printf("%s: SPI device add failed: %s\n", TAG, esp_err_to_name(ret));
        return;
    }
    // printf("%s: SPI device added successfully\n", TAG);

    // 硬件复位
    gpio_set_level(TFT_RST, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(TFT_RST, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // 软件复位
    spi_write_command(0x01); // SWRESET
    vTaskDelay(150 / portTICK_PERIOD_MS);

    // 初始化ST7735S
    st7735s_init_sequence();

    // 背光开
    tft_backlight_on();

    // printf("%s: TFT init complete\n", TAG);
}

/**
 * @brief ST7735S TFT控制器初始化序列
 * 
 * 该函数按照ST7735S数据手册规定的顺序发送一系列命令和参数，
 * 用于配置TFT控制器的各项参数，包括帧率控制、电源管理、
 * 显示方向、像素格式和伽马校正等，最终使能显示。
 */
static void st7735s_init_sequence() 
{
    // 退出睡眠模式，唤醒显示器
    spi_write_command(0x11); // Sleep out
    vTaskDelay(120 / portTICK_PERIOD_MS); // 等待120ms，确保显示器完全唤醒

    // 帧率控制 - 正常模式全色
    spi_write_command(0xB1); // Frame rate control
    spi_write_data(0x01); // 分频比
    spi_write_data(0x2C); // 16个时钟周期
    spi_write_data(0x2D); // 16个时钟周期

    // 帧率控制 - 空闲模式
    spi_write_command(0xB2); // Frame rate control
    spi_write_data(0x01); // 分频比
    spi_write_data(0x2C); // 16个时钟周期
    spi_write_data(0x2D); // 16个时钟周期

    // 帧率控制 - 部分模式全色和空闲模式
    spi_write_command(0xB3); // Frame rate control
    spi_write_data(0x01); // 部分模式全色分频比
    spi_write_data(0x2C); // 部分模式全色16个时钟周期
    spi_write_data(0x2D); // 部分模式全色16个时钟周期
    spi_write_data(0x01); // 空闲模式分频比
    spi_write_data(0x2C); // 空闲模式16个时钟周期
    spi_write_data(0x2D); // 空闲模式16个时钟周期

    // 列反转控制
    spi_write_command(0xB4); // Column inversion
    spi_write_data(0x07); // 列反转设置

    // 电源控制1 - GVDD级别设置
    spi_write_command(0xC0); // Power control
    spi_write_data(0xA2); // 设置GVDD
    spi_write_data(0x02); // 设置增强因子
    spi_write_data(0x84); // 设置偏置电压

    // 电源控制2 - VGH和VGL设置
    spi_write_command(0xC1); // Power control
    spi_write_data(0xC5); // 设置VGH和VGL电平

    // 电源控制3 - 操作模式设置
    spi_write_command(0xC2); // Power control
    spi_write_data(0x0A); // 设置操作模式
    spi_write_data(0x00); // 保留位

    // 电源控制4 - 在正常模式下的VGH和VGL设置
    spi_write_command(0xC3); // Power control
    spi_write_data(0x8A); // 设置VGH和VGL电平
    spi_write_data(0x2A); // 设置VREG1OUT电压

    // 电源控制5 - 在部分模式下的VGH和VGL设置
    spi_write_command(0xC4); // Power control
    spi_write_data(0x8A); // 设置VGH和VGL电平
    spi_write_data(0xEE); // 设置VREG2OUT电压

    // VCOM控制 - 设置VCOM电压
    spi_write_command(0xC5); // VCOM control
    spi_write_data(0x0E); // 设置VCOM电压

    // 内存数据访问控制 - 设置显示方向和颜色顺序
    spi_write_command(0x36); // Memory data access control
    spi_write_data(0xB0); // BGR模式，无行/列交换

    // 接口像素格式 - 设置颜色深度
    spi_write_command(0x3A); // Interface pixel format
    spi_write_data(0x05); // 16位/像素 (RGB565格式)

    // 正伽马校正 - 设置正极性伽马曲线
    spi_write_command(0xE0); // GMCTRP1
    spi_write_data(0x02); spi_write_data(0x1C); spi_write_data(0x07); spi_write_data(0x12);
    spi_write_data(0x37); spi_write_data(0x32); spi_write_data(0x29); spi_write_data(0x2D);
    spi_write_data(0x29); spi_write_data(0x25); spi_write_data(0x2B); spi_write_data(0x39);
    spi_write_data(0x00); spi_write_data(0x01); spi_write_data(0x03); spi_write_data(0x10);

    // 负伽马校正 - 设置负极性伽马曲线
    spi_write_command(0xE1); // GMCTRN1
    spi_write_data(0x03); spi_write_data(0x1D); spi_write_data(0x07); spi_write_data(0x06);
    spi_write_data(0x2E); spi_write_data(0x2C); spi_write_data(0x29); spi_write_data(0x2D);
    spi_write_data(0x2E); spi_write_data(0x2E); spi_write_data(0x37); spi_write_data(0x3F);
    spi_write_data(0x00); spi_write_data(0x00); spi_write_data(0x02); spi_write_data(0x10);

    // 正常显示模式开启
    spi_write_command(0x13); // Normal display mode on

    // 开启显示 - 使能显示器
    spi_write_command(0x29); // Display on
}

/**
 * @brief 通过SPI发送8位命令到TFT控制器
 * 
 * 该函数用于发送单字节命令到ST7735S TFT控制器。在发送命令前，
 * 会先将DC（数据/命令）引脚设置为低电平，表示接下来传输的是命令而非数据。
 * 
 * @param cmd 要发送的8位命令代码
 */
static void spi_write_command(uint8_t cmd) 
{
    // 设置DC引脚为低电平，表示接下来传输的是命令而非数据
    // ST7735S控制器通过DC引脚区分命令和数据
    gpio_set_level(TFT_DC, 0); // Command mode
    
    // 配置SPI传输事务
    spi_transaction_t t = {
        .length = 8,         // 传输8位数据（1字节）
        .tx_buffer = &cmd    // 指向包含命令字节的缓冲区
    };
    
    // 执行SPI传输，将命令发送到TFT控制器
    // 这是一个阻塞调用，函数会在传输完成后返回
    spi_device_transmit(spi_handle, &t);
}

/**
 * @brief 通过SPI发送8位数据到TFT控制器
 * 
 * 该函数用于发送单字节数据到ST7735S TFT控制器。在发送数据前，
 * 会先将DC（数据/命令）引脚设置为高电平，表示接下来传输的是数据而非命令。
 * 
 * @param data 要发送的8位数据
 */
static void spi_write_data(uint8_t data) 
{
    gpio_set_level(TFT_DC, 1); // Data mode
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data
    };
    spi_device_transmit(spi_handle, &t);
}

/**
 * @brief 通过SPI发送16位数据到TFT控制器
 * 
 * 该函数用于发送16位颜色数据（RGB565格式）或其他16位参数到TFT控制器。
 * 它将16位数据分解为两个8位字节，然后通过SPI接口发送。
 * 
 * @param data 要发送的16位数据
 */
static void spi_write_data16(uint16_t data) 
{
    // 将16位数据分解为两个8位字节
    // 高字节在前（大端序），低字节在后
    uint8_t buf[2] = {data >> 8, data & 0xFF};
    
    // 设置DC引脚为高电平，表示接下来传输的是数据而非命令
    // ST7735S控制器通过DC引脚区分命令和数据
    gpio_set_level(TFT_DC, 1); // Data mode
    
    // 配置SPI传输事务
    spi_transaction_t t = {
        .length = 16,        // 传输16位数据（2字节）
        .tx_buffer = buf     // 指向包含两个字节的缓冲区
    };
    
    // 执行SPI传输，将数据发送到TFT控制器
    // 这是一个阻塞调用，函数会在传输完成后返回
    spi_device_transmit(spi_handle, &t);
}

/**
 * @brief 设置TFT屏幕的地址窗口
 * 
 * @param x0 起始X坐标
 * @param y0 起始Y坐标
 * @param x1 结束X坐标
 * @param y1 结束Y坐标
 * 
 * 该函数通过发送列地址和行地址设置命令，定义了后续像素数据写入的区域。
 */
static void tft_set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) 
{
    // 确保坐标不超出屏幕范围
    if (x1 >= TFT_WIDTH) x1 = TFT_WIDTH - 1;
    if (y1 >= TFT_HEIGHT) y1 = TFT_HEIGHT - 1;

    spi_write_command(0x2A); // Column address set
    spi_write_data16(x0 + 2); // Offset for 1.44" display
    spi_write_data16(x1 + 2);

    spi_write_command(0x2B); // Row address set
    spi_write_data16(y0); // No y offset
    spi_write_data16(y1);

    spi_write_command(0x2C); // Memory write
}

/**
 * @brief 在TFT屏幕上绘制单个像素点
 * 
 * @param x 像素的X坐标（0到TFT_WIDTH-1）
 * @param y 像素的Y坐标（0到TFT_HEIGHT-1）
 * @param color 像素颜色，使用RGB565格式
 * 
 * 该函数首先检查坐标是否在屏幕范围内，然后设置地址窗口到指定像素位置，
 * 最后通过SPI发送16位颜色数据来绘制该像素点。
 */
static void tft_draw_pixel(uint16_t x, uint16_t y, uint16_t color) 
{
    // 检查坐标是否超出屏幕范围，如果超出则直接返回
    if (x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
    
    // 设置地址窗口到指定像素位置，x0=x1=x, y0=y1=y
    tft_set_address_window(x, y, x, y);
    
    // 通过SPI发送16位颜色数据（RGB565格式）
    spi_write_data16(color);
}

/**
 * @brief 高效填充整个TFT屏幕
 * 
 * 性能优化策略：
 * 1. 使用单行缓冲区减少内存占用
 * 2. 利用TFT控制器的连续写入能力
 * 3. 通过DMA传输提高SPI通信效率
 * 
 * @param color RGB565格式的颜色值
 */
static void tft_fill_screen(uint16_t color) 
{
    // 步骤1: 设置写入区域为整个屏幕
    tft_set_address_window(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);
    
    // 步骤2: 切换到数据模式
    gpio_set_level(TFT_DC, 1);
    
    // 分块大小，避免单次传输过大
    const int CHUNK_SIZE = 32;  // 每次传输32像素，进一步减小
    uint16_t buf[CHUNK_SIZE];
    
    // 预填充缓冲区
    for (int i = 0; i < CHUNK_SIZE; i++) buf[i] = color;
    
    // 计算总像素数
    int total_pixels = TFT_WIDTH * TFT_HEIGHT;
    int pixels_sent = 0;
    
    while (pixels_sent < total_pixels) {
        int pixels_to_send = (total_pixels - pixels_sent) > CHUNK_SIZE ? CHUNK_SIZE : (total_pixels - pixels_sent);
        
        spi_transaction_t t = {
            .length = pixels_to_send * 16,
            .tx_buffer = buf
        };
        
        esp_err_t ret = spi_device_transmit(spi_handle, &t);
        if (ret != ESP_OK) {
            // ESP_LOGE(TAG, "SPI transmit failed at pixel %d: %s", pixels_sent, esp_err_to_name(ret));
            break;
        }
        
        pixels_sent += pixels_to_send;
    }
}

/**
 * @brief 在TFT屏幕上绘制矩形
 * 
 * @param x 矩形左上角的X坐标（0到TFT_WIDTH-1）
 * @param y 矩形左上角的Y坐标（0到TFT_HEIGHT-1）
 * @param w 矩形的宽度（0到TFT_WIDTH-x）
 * @param h 矩形的高度（0到TFT_HEIGHT-y）
 * @param color 矩形颜色，使用RGB565格式
 * 
 * 该函数首先检查矩形是否超出屏幕范围，如果超出则调整宽度和高度，
 * 然后设置地址窗口到指定矩形区域，最后通过SPI发送颜色数据来绘制矩形。
 */
static void tft_draw_rectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) 
{
    if (x + w > TFT_WIDTH) w = TFT_WIDTH - x;
    if (y + h > TFT_HEIGHT) h = TFT_HEIGHT - y;
    tft_set_address_window(x, y, x + w - 1, y + h - 1);
    gpio_set_level(TFT_DC, 1);
    
    const int CHUNK_SIZE = 32;
    uint16_t buf[CHUNK_SIZE];
    for (int i = 0; i < CHUNK_SIZE; i++) buf[i] = color;
    
    int total_pixels = w * h;
    int pixels_sent = 0;
    
    while (pixels_sent < total_pixels) {
        int pixels_to_send = (total_pixels - pixels_sent) > CHUNK_SIZE ? CHUNK_SIZE : (total_pixels - pixels_sent);
        
        spi_transaction_t t = {
            .length = pixels_to_send * 16,
            .tx_buffer = buf
        };
        
        esp_err_t ret = spi_device_transmit(spi_handle, &t);
        if (ret != ESP_OK) {
            // ESP_LOGE(TAG, "SPI transmit failed in rectangle: %s", esp_err_to_name(ret));
            break;
        }
        
        pixels_sent += pixels_to_send;
    }
}

static void tft_backlight_on() 
{
    gpio_set_level(TFT_BLK, 1);
}

static void tft_backlight_off() 
{
    gpio_set_level(TFT_BLK, 0);
}


