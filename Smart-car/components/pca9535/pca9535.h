#ifndef __PCA9535_H__
#define __PCA9535_H__

#include "MyApplication.h"

/* Private define-------------------------------------------------------------*/
#define PCA9535_ADDR 0x20

/* Enum-----------------------------------------------------------------------*/
typedef enum {
    PCA_PIN_0_0 = 0, PCA_PIN_0_1, PCA_PIN_0_2, PCA_PIN_0_3, PCA_PIN_0_4, PCA_PIN_0_5, PCA_PIN_0_6, PCA_PIN_0_7,
    PCA_PIN_1_0,     PCA_PIN_1_1, PCA_PIN_1_2, PCA_PIN_1_3, PCA_PIN_1_4, PCA_PIN_1_5, PCA_PIN_1_6, PCA_PIN_1_7
} PCA_Pin_t;

/* Struct---------------------------------------------------------------------*/
typedef struct
{
    void (*init)(void);
    void (*set_pin)(PCA_Pin_t pin, uint8_t level);
    uint8_t (*get_pin)(PCA_Pin_t pin);
} PCA9535_t;

/* extern variables-----------------------------------------------------------*/
extern PCA9535_t PCA_Extender;

/* Public function declarations-----------------------------------------------*/
esp_err_t pca9535_pre_reset_safe_config(void);
bool pca9535_is_ready(void);

#endif
/********************************************************
  End Of File
********************************************************/