#ifndef __BSP_I2C_H__
#define __BSP_I2C_H__

#include "MyApplication.h"
#include "driver/i2c.h"

/* Struct---------------------------------------------------------------------*/
typedef struct
{
    void (*init)(void);
} BSP_I2C_t;

/* extern variables-----------------------------------------------------------*/
extern BSP_I2C_t I2C_Master;

#endif
/********************************************************
  End Of File
********************************************************/