#ifndef __WIFI_APP_H__
#define __WIFI_APP_H__

#include "MyApplication.h"

/* Struct---------------------------------------------------------------------*/
typedef struct
{
    void (*init)(void);
} WiFi_App_t;

/* extern variables-----------------------------------------------------------*/
extern WiFi_App_t WiFiApp;

#endif
/********************************************************
  End Of File
********************************************************/