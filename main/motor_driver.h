#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include "driver/gpio.h"
#include "driver/ledc.h"

// TB6612FNG1控制引脚定义
#define MOTOR_AIN1_GPIO_LF      GPIO_NUM_19
#define MOTOR_AIN2_GPIO_LF      GPIO_NUM_20
#define MOTOR_STBY_GPIO         GPIO_NUM_10
#define MOTOR_PWMA_CHANNEL_LF   LEDC_CHANNEL_1  // 使用不同的通道，避免与 LED 通道冲突
#define MOTOR_PWMA_GPIO_LF      GPIO_NUM_1
#define MOTOR_PWM_TIMER         LEDC_TIMER_1    // 采用 TIMER_1

#define MOTOR_BIN1_GPIO_LB      GPIO_NUM_19
#define MOTOR_BIN2_GPIO_LB      GPIO_NUM_20
#define MOTOR_PWMB_CHANNEL_LB   LEDC_CHANNEL_2  // 使用不同的通道，避免与 LED 通道冲突
#define MOTOR_PWMB_GPIO_LB      GPIO_NUM_2

// TB6612FNG1控制引脚定义
#define MOTOR_AIN1_GPIO_RF      GPIO_NUM_19  
#define MOTOR_AIN2_GPIO_RF      GPIO_NUM_20
#define MOTOR_PWMA_CHANNEL_RF   LEDC_CHANNEL_3  // 使用不同的通道，避免与 LED 通道冲突
#define MOTOR_PWMA_GPIO_RF      GPIO_NUM_5

#define MOTOR_BIN1_GPIO_RB      GPIO_NUM_19  
#define MOTOR_BIN2_GPIO_RB      GPIO_NUM_20
#define MOTOR_PWMB_CHANNEL_RB   LEDC_CHANNEL_4  // 使用不同的通道，避免与 LED 通道冲突
#define MOTOR_PWMB_GPIO_RB      GPIO_NUM_6

// 初始化电机驱动
void motor_driver_init();

// 设置电机速度和方向
// speed: 0-100% 速度
// direction: 0=停止, 1=正转, 2=反转
void set_motor_speed(uint8_t speed, uint8_t direction);

#endif // MOTOR_DRIVER_H
