#ifndef __CAMERA_H__
#define __CAMERA_H__

#include "MyApplication.h"
#include "esp_camera.h"

/* Struct---------------------------------------------------------------------*/
typedef struct
{
    void (*init)(void);
    camera_fb_t* (*get_frame)(void);
    void (*return_frame)(camera_fb_t *fb);
} Camera_t;

/* extern variables-----------------------------------------------------------*/
extern Camera_t CarCamera;

#endif
/********************************************************
  End Of File
********************************************************/