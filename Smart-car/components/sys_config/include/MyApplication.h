#ifndef __MYAPPLICATION_H__
#define __MYAPPLICATION_H__

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

//定义枚举类型 -> TRUE/FALSE位
typedef enum 
{
  FALSE = 0U, 
  TRUE = !FALSE
} FlagStatus_t;

/* Global Definitions---------------------------------------------------------*/
#define PCA9535_I2C_ADDR    0x20
#define OV2640_I2C_ADDR     0x30
#define OLED_I2C_ADDR       0x3C  // 7-bit address (0x78 is the 8-bit write form)

// I2C Pins
#define I2C_MASTER_SCL_IO   42
#define I2C_MASTER_SDA_IO   41
#define I2C_PORT_NUM        I2C_NUM_0

// Motor Encoders
#define MOTOR_LF_ENC_A      38
#define MOTOR_LF_ENC_B      39
#define MOTOR_LR_ENC_A      40
#define MOTOR_LR_ENC_B      18
#define MOTOR_RF_ENC_A      8
#define MOTOR_RF_ENC_B      15
#define MOTOR_RR_ENC_A      16
#define MOTOR_RR_ENC_B      17

// Motor PWM
#define MOTOR_L_PWMA        1
#define MOTOR_L_PWMB        2
#define MOTOR_R_PWMA        5
#define MOTOR_R_PWMB        6

// Servo
#define SERVO_PIN           7

// Camera Pins
#define CAM_VSYNC           45
#define CAM_HREF            48
#define CAM_RST             19
#define CAM_D0              47
#define CAM_D1              14
#define CAM_D2              21
#define CAM_D3              12
#define CAM_D4              13
#define CAM_D5              10
#define CAM_D6              11
#define CAM_D7              3
#define CAM_PCLK            9
#define CAM_XCLK            4
#define CAM_PWDN            46

#endif /* __MYAPPLICATION_H__ */
/********************************************************
  End Of File
********************************************************/
