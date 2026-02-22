#ifndef __ENCODER_H__
#define __ENCODER_H__

#include "driver/pcnt.h"

// 霍尔编码器引脚定义
//左前轮
#define ENCODER_A1_GPIO         GPIO_NUM_38
#define ENCODER_B1_GPIO         GPIO_NUM_39
#define PCNT_UNIT_LF            PCNT_UNIT_0

//左后轮
#define ENCODER_A2_GPIO          GPIO_NUM_40
#define ENCODER_B2_GPIO          GPIO_NUM_18
#define PCNT_UNIT_LB             PCNT_UNIT_1

//右前轮
#define ENCODER_A3_GPIO          GPIO_NUM_8
#define ENCODER_B3_GPIO          GPIO_NUM_15
#define PCNT_UNIT_RF             PCNT_UNIT_2

//右后轮
#define ENCODER_A4_GPIO          GPIO_NUM_16
#define ENCODER_B4_GPIO          GPIO_NUM_17
#define PCNT_UNIT_RB             PCNT_UNIT_3

#define ENCODER_PULSES_PER_REV (11*48)  // 每转脉冲数，11齿轮配48脉冲/齿轮

// 编码器状态结构体
typedef struct {
    int32_t pulse_count;   // 本周期脉冲计数
    float rpm;             // 转速 (RPM)
    float rotations;       // 本周期圈数
} encoder_status_t;

// 定义结构体类型
typedef struct {
    void (*encoder_init)(void);                    // 初始化编码器
    encoder_status_t (*encoder_get_status)(void);  // 获取编码器状态
} Encoder_t;

/* extern variables-----------------------------------------------------------*/
extern Encoder_t Encoder;

#endif // __ENCODER_H__
