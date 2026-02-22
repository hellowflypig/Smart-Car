/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "MyApplication.h"
#include "ov2640.h"
#include "uart.h"
#include "lcd.h"

/* Private define-------------------------------------------------------------*/
#define TAG "OV2640"

/* Private variables----------------------------------------------------------*/
static camera_config_t camera_config;

/* Private function prototypes------------------------------------------------*/
static void ov2640_init();
static camera_fb_t* ov2640_capture_frame();
static void ov2640_display_to_tft(camera_fb_t* fb);
static void ov2640_free_frame(camera_fb_t* fb);
static void ov2640_start_uart_stream(uint32_t interval_ms);

/* Public variables-----------------------------------------------------------*/
OV2640_t OV2640 = {
    ov2640_init,
    ov2640_capture_frame,
    ov2640_display_to_tft,
    ov2640_free_frame,
    ov2640_start_uart_stream
};

/* Private functions ---------------------------------------------------------*/
static void ov2640_init() {
    // printf("%s: Initializing OV2640 camera\n", TAG);

    camera_config.ledc_channel = LEDC_CHANNEL_0;
    camera_config.ledc_timer = LEDC_TIMER_0;
    camera_config.pin_d0 = CAM_PIN_D0;
    camera_config.pin_d1 = CAM_PIN_D1;
    camera_config.pin_d2 = CAM_PIN_D2;
    camera_config.pin_d3 = CAM_PIN_D3;
    camera_config.pin_d4 = CAM_PIN_D4;
    camera_config.pin_d5 = CAM_PIN_D5;
    camera_config.pin_d6 = CAM_PIN_D6;
    camera_config.pin_d7 = CAM_PIN_D7;
    camera_config.pin_xclk = CAM_PIN_XCLK;
    camera_config.pin_pclk = CAM_PIN_PCLK;
    camera_config.pin_vsync = CAM_PIN_VSYNC;
    camera_config.pin_href = CAM_PIN_HREF;
    camera_config.pin_sccb_sda = CAM_PIN_SIOD;
    camera_config.pin_sccb_scl = CAM_PIN_SIOC;
    camera_config.xclk_freq_hz = 20000000;  // 20MHz for high framerate
    // Use JPEG format for streaming over UART
    camera_config.pixel_format = PIXFORMAT_JPEG;
    camera_config.frame_size = FRAME_SIZE;
    camera_config.jpeg_quality = 12;  // Not used for RGB
    camera_config.fb_count = 1;  // Single buffer for low latency
    camera_config.fb_location = CAMERA_FB_IN_PSRAM;  // Use PSRAM if available
    camera_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    // If a RESET pin is defined, drive a reset pulse before init
    if (CAM_PIN_RESET != -1) {
        gpio_config_t rst_conf = {
            .pin_bit_mask = (1ULL << CAM_PIN_RESET),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&rst_conf);
        // Hold reset low briefly then release
        gpio_set_level(CAM_PIN_RESET, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(CAM_PIN_RESET, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Initialize camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        printf("%s: Camera init failed: %s\n", TAG, esp_err_to_name(err));
        return;
    }

    printf("%s: OV2640 initialized successfully\n", TAG);
}

static camera_fb_t* ov2640_capture_frame() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        // printf("%s: Failed to capture frame\n", TAG);
        return NULL;
    }
    return fb;
}

static void ov2640_display_to_tft(camera_fb_t* fb) {
    if (!fb) return;
    if (fb->format != PIXFORMAT_RGB565) return;

    // 安全检查帧缓冲长度（RGB565 每像素 2 字节）
    uint32_t fb_width = fb->width;
    uint32_t fb_height = fb->height;
    uint32_t expected_len = fb_width * fb_height * 2;
    if (fb->len < expected_len) return;

    uint16_t* img_buf = (uint16_t*)fb->buf;

    // 使用帧缓冲实际宽高做缩放映射，避免越界
    for (int y = 0; y < TFT_HEIGHT; y++) {
        for (int x = 0; x < TFT_WIDTH; x++) {
            int src_x = (x * (int)fb_width) / TFT_WIDTH;
            int src_y = (y * (int)fb_height) / TFT_HEIGHT;
            if (src_x < 0) src_x = 0;
            if (src_x >= (int)fb_width) src_x = fb_width - 1;
            if (src_y < 0) src_y = 0;
            if (src_y >= (int)fb_height) src_y = fb_height - 1;
            uint16_t color = img_buf[src_y * fb_width + src_x];
            TFT.tft_draw_pixel(x, y, color);
        }
    }
}

static void ov2640_free_frame(camera_fb_t* fb) {
    if (fb) {
        esp_camera_fb_return(fb);
    }
}

// --- UART streaming -------------------------------------------------------
// Frame protocol: 4-byte magic "XIMG" then 4-byte big-endian length, then JPEG data
static void ov2640_uart_stream_task(void *arg) {
    uint32_t interval_ms = (uint32_t)(uintptr_t)arg;
    if (interval_ms == 0) interval_ms = 1000; // default 1s

    // Ensure UART initialized
    uart0_init();

    while (1) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            printf("ov2640_uart_stream: capture failed\n");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // send header
        const char magic[4] = {'X','I','M','G'};
        uart0_send_data((const uint8_t*)magic, 4);

        uint32_t len = fb->len;
        uint8_t len_buf[4];
        len_buf[0] = (len >> 24) & 0xFF;
        len_buf[1] = (len >> 16) & 0xFF;
        len_buf[2] = (len >> 8) & 0xFF;
        len_buf[3] = (len) & 0xFF;
        uart0_send_data(len_buf, 4);

        // send payload (JPEG)
        uart0_send_data((const uint8_t*)fb->buf, fb->len);

        esp_camera_fb_return(fb);

        vTaskDelay(pdMS_TO_TICKS(interval_ms));
    }

    vTaskDelete(NULL);
}

static void ov2640_start_uart_stream(uint32_t interval_ms) {
    xTaskCreatePinnedToCore(ov2640_uart_stream_task, "cam_uart_stream", 4096, (void*)(uintptr_t)interval_ms, tskIDLE_PRIORITY+3, NULL, 1);
}