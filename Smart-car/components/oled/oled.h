#ifndef __OLED_H__
#define __OLED_H__

#include "MyApplication.h"

/* Struct---------------------------------------------------------------------*/
typedef struct
{
    void (*init)(void);
    void (*clear)(void);
    void (*show_string)(uint8_t x, uint8_t y, const char *str, uint8_t size);
    void (*show_num)(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size);
    void (*refresh)(void);
} OLED_t;

/* extern variables-----------------------------------------------------------*/
extern OLED_t OLED;

  bool oled_set_i2c_addr(uint8_t addr);
  uint8_t oled_get_i2c_addr(void);

#endif
/********************************************************
  End Of File
********************************************************/