#ifndef __MOTOR_H__
#define __MOTOR_H__

#include "MyApplication.h"
#include "pca9535.h"

/* Enum-----------------------------------------------------------------------*/
typedef enum {
    MOTOR_LF = 0,
    MOTOR_LR,
    MOTOR_RF,
    MOTOR_RR
} Motor_ID_t;

/* Struct---------------------------------------------------------------------*/
typedef struct
{
    void (*init)(void);
    void (*set_speed)(Motor_ID_t id, int speed); // For open loop
    void (*set_target)(Motor_ID_t id, int32_t target_pos, int32_t target_speed); // For closed loop
} Motor_t;

/* extern variables-----------------------------------------------------------*/
extern Motor_t CarMotor;

int32_t motor_get_position(Motor_ID_t id);
void motor_get_all_positions(int32_t pos_out[4]);

#endif
/********************************************************
  End Of File
********************************************************/