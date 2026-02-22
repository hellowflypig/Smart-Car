#ifndef __LED_PWM_H__
#define __LED_PWM_H__

#include "driver/ledc.h"

#define LED0 4
#define LED1 5

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT // 13位分辨率，最大8191
#define LEDC_FREQUENCY 5000 // 5kHz

//定义结构体类型
typedef struct {
    void (*led_init)(void);          // 初始化LED PWM
    void (*led_on)(uint32_t gpio_num);  // 打开LED (100%亮度)
    void (*led_off)(uint32_t gpio_num); // 关闭LED (0%亮度)
    void (*led_flip)(uint32_t gpio_num); // 切换LED状态 (0% 或 100%)
    void (*led_set_brightness)(uint32_t gpio_num, uint32_t brightness); // 设置亮度 (0-255)
    void (*led_fade_brightness)(uint32_t gpio_num, uint32_t start_duty, uint32_t end_duty, uint32_t fade_time_ms); // 硬件渐变亮度
} LED_t;

/* extern variables-----------------------------------------------------------*/
extern LED_t  LED;

int led_get_brightness(uint32_t gpio_num);

#endif