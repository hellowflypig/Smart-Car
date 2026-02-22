#ifndef __OV2640_H__
#define __OV2640_H__

#include "esp_camera.h"

// OV2640 引脚定义（按用户提供的原理图映射）
#define CAM_PIN_PCLK    GPIO_NUM_9
#define CAM_PIN_XCLK    GPIO_NUM_4
#define CAM_PIN_PWDN    GPIO_NUM_46
// CAM_PIN_RESET 与模块 EN 相连（由板子上电控制），这里不使用独立 RESET GPIO
#define CAM_PIN_RESET   GPIO_NUM_19

// 数据线 D0..D7
#define CAM_PIN_D0      GPIO_NUM_47
#define CAM_PIN_D1      GPIO_NUM_14
#define CAM_PIN_D2      GPIO_NUM_21
#define CAM_PIN_D3      GPIO_NUM_12
#define CAM_PIN_D4      GPIO_NUM_13
#define CAM_PIN_D5      GPIO_NUM_10
#define CAM_PIN_D6      GPIO_NUM_11
#define CAM_PIN_D7      GPIO_NUM_3

// SCCB (I2C) 总线
#define CAM_PIN_SIOD    GPIO_NUM_41  // SDA
#define CAM_PIN_SIOC    GPIO_NUM_42  // SCL

#define CAM_PIN_VSYNC   GPIO_NUM_45
#define CAM_PIN_HREF    GPIO_NUM_48
#define CAM_PIN_FLASH   GPIO_NUM_48  // 若需要闪光灯控制，可与 HREF 不同；保持原值或按板子调整

// 摄像头参数
#define CAM_WIDTH       320
#define CAM_HEIGHT      240
#define FRAME_SIZE      FRAMESIZE_QVGA  // 320x240 for high framerate

// 定义结构体类型
typedef struct {
    void (*ov2640_init)(void);                          // 初始化OV2640
    camera_fb_t* (*ov2640_capture_frame)(void);         // 捕获一帧
    void (*ov2640_display_to_tft)(camera_fb_t* fb);     // 显示帧到TFT
    void (*ov2640_free_frame)(camera_fb_t* fb);         // 释放帧缓冲
    void (*ov2640_start_uart_stream)(uint32_t interval_ms); // 开始串口推送图像，间隔 ms
} OV2640_t;

/* extern variables-----------------------------------------------------------*/
extern OV2640_t OV2640;

#endif