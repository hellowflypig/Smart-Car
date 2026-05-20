/* Includes ------------------------------------------------------------------*/
#include "camera.h"

/* Private macros ------------------------------------------------------------*/
#define OV2640_PID_EXPECTED 0x26

/* Private variables----------------------------------------------------------*/
static const char *TAG = "CAMERA";

/* Private function prototypes------------------------------------------------*/
static void camera_sys_init(void);
static camera_fb_t* camera_sys_get_frame(void);
static void camera_sys_return_frame(camera_fb_t *fb);

/* Public variables-----------------------------------------------------------*/
Camera_t CarCamera = {
    camera_sys_init,
    camera_sys_get_frame,
    camera_sys_return_frame
};

/* Private function definitions-----------------------------------------------*/

/*
    * @name   camera_sys_init
    * @brief  Initialize OV2640 via esp32-camera component
    * @param  None
    * @retval None      
*/
static void camera_sys_init(void)
{
    ESP_LOGI(TAG, "Initializing OV2640 Camera...");

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_5; // motor uses 0-3, servo uses 4
    config.ledc_timer = LEDC_TIMER_2;
    config.pin_d0 = CAM_D0;
    config.pin_d1 = CAM_D1;
    config.pin_d2 = CAM_D2;
    config.pin_d3 = CAM_D3;
    config.pin_d4 = CAM_D4;
    config.pin_d5 = CAM_D5;
    config.pin_d6 = CAM_D6;
    config.pin_d7 = CAM_D7;
    config.pin_xclk = CAM_XCLK;
    config.pin_pclk = CAM_PCLK;
    config.pin_vsync = CAM_VSYNC;
    config.pin_href = CAM_HREF;
    config.pin_sccb_sda = I2C_MASTER_SDA_IO; // Using same I2C pins as master
    config.pin_sccb_scl = I2C_MASTER_SCL_IO;
    config.pin_pwdn = CAM_PWDN;
    config.pin_reset = CAM_RST;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG; // JPEG for WiFi stream
    
    // Set frame size according to required FPS (VGA 640x480 or QVGA 320x240)
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12; // 10-63, lower means higher quality
    config.fb_count = 2; // Double buffering for higher FPS
    config.fb_location = CAMERA_FB_IN_PSRAM; // Store frame buffer in PSRAM
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    
    // Camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed! Error: 0x%x", err);
        return;
    }

    // Get sensor reference to tune parameters
    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        ESP_LOGE(TAG, "SCCB probe failed: sensor handle is NULL");
        return;
    }

    ESP_LOGI(TAG,
             "SCCB probe OK: MID=0x%02X%02X PID=0x%02X VER=0x%02X",
             s->id.MIDH,
             s->id.MIDL,
             s->id.PID,
             s->id.VER);
    if (s->id.PID != OV2640_PID_EXPECTED) {
        ESP_LOGW(TAG,
                 "Sensor PID mismatch, expected OV2640(0x%02X), got 0x%02X",
                 OV2640_PID_EXPECTED,
                 s->id.PID);
    }

    s->set_vflip(s, 1); // Flip vertically if needed by hardware mounting
    s->set_hmirror(s, 1);

    // Capture one frame during init to verify data path is alive.
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == NULL) {
        ESP_LOGE(TAG, "Frame self-test failed: fb_get returned NULL");
        return;
    }

    bool jpeg_valid = (fb->len >= 4 &&
                       fb->buf[0] == 0xFF && fb->buf[1] == 0xD8 &&
                       fb->buf[fb->len - 2] == 0xFF && fb->buf[fb->len - 1] == 0xD9);
    ESP_LOGI(TAG,
             "Frame self-test OK: len=%u, jpeg=%s",
             (unsigned int)fb->len,
             jpeg_valid ? "yes" : "no");
    esp_camera_fb_return(fb);
    
    ESP_LOGI(TAG, "Camera Init Successful");
}

/*
    * @name   camera_sys_get_frame
    * @brief  Get a frame buffer from the camera
*/
static camera_fb_t* camera_sys_get_frame(void)
{
    return esp_camera_fb_get();
}

/*
    * @name   camera_sys_return_frame
    * @brief  Return the frame buffer to the camera driver
*/
static void camera_sys_return_frame(camera_fb_t *fb)
{
    if(fb) {
        esp_camera_fb_return(fb);
    }
}

/********************************************************
  End Of File
********************************************************/