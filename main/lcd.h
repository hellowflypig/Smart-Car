#ifndef __TFT_H__
#define __TFT_H__

#include "driver/spi_master.h"

// TFT 引脚定义
#define TFT_CS   10
#define TFT_DC   11
#define TFT_RST  12
#define TFT_BLK  13
#define TFT_SCL  14  // SPI CLK
#define TFT_SDA  15  // SPI MOSI

// TFT 参数
#define TFT_WIDTH   128
#define TFT_HEIGHT  128

// 颜色定义 (RGB565)
#define TFT_BLACK       0x0000
#define TFT_WHITE       0xFFFF
#define TFT_RED         0xF800
#define TFT_GREEN       0x07E0
#define TFT_BLUE        0x001F
#define TFT_YELLOW      0xFFE0
#define TFT_CYAN        0x07FF
#define TFT_MAGENTA     0xF81F

// 定义结构体类型
typedef struct {
    void (*tft_init)(void);                              // 初始化TFT
    void (*tft_set_address_window)(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1); // 设置地址窗口
    void (*tft_draw_pixel)(uint16_t x, uint16_t y, uint16_t color); // 绘制像素
    void (*tft_fill_screen)(uint16_t color);             // 填充屏幕
    void (*tft_draw_rectangle)(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color); // 绘制矩形
    void (*tft_backlight_on)(void);                      // 背光开
    void (*tft_backlight_off)(void);                     // 背光关
} TFT_t;

/* extern variables-----------------------------------------------------------*/
extern TFT_t TFT;

#endif