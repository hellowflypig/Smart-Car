#ifndef __SERVO_H__
#define __SERVO_H__

#include "MyApplication.h"

/* Struct---------------------------------------------------------------------*/
typedef struct
{
    void (*init)(void);
    void (*set_angle)(float angle); // Angle from 0 to 180
} Servo_t;

/* extern variables-----------------------------------------------------------*/
extern Servo_t CameraServo;

#endif
/********************************************************
  End Of File
********************************************************/