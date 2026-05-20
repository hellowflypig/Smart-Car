/* Includes ------------------------------------------------------------------*/
#include "MyApplication.h"
#include "bsp_i2c.h"
#include "oled.h"
#include "camera.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "esp_system.h"

/* Private variables----------------------------------------------------------*/
static const char *TAG = "CAM_UART_TEST";

/* UART config for XCAM serial JPEG mode. */
#define XCAM_UART_PORT             UART_NUM_0
#define XCAM_UART_BAUD             921600
#define XCAM_UART_TX_BUF_SIZE      (24 * 1024)
#define XCAM_UART_WRITE_TIMEOUT_MS 1500
#define STREAM_FRAME_INTERVAL_MS   40

/* Private function prototypes------------------------------------------------*/
static const char *reset_reason_to_str(esp_reset_reason_t reason);
static bool i2c_probe_7bit_addr(uint8_t addr);
static bool oled_link_self_test(void);
static void xcam_uart_init(void);
static bool jpeg_frame_is_valid(const camera_fb_t *fb);
static uint32_t frame_quick_signature(const camera_fb_t *fb);
static bool camera_link_self_test(void);
static esp_err_t uart_send_jpeg_frame(const camera_fb_t *fb);

/* Private function definitions-----------------------------------------------*/
static const char *reset_reason_to_str(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_POWERON:
            return "POWERON";
        case ESP_RST_SW:
            return "SW";
        case ESP_RST_PANIC:
            return "PANIC";
        case ESP_RST_INT_WDT:
            return "INT_WDT";
        case ESP_RST_TASK_WDT:
            return "TASK_WDT";
        case ESP_RST_WDT:
            return "WDT";
        case ESP_RST_BROWNOUT:
            return "BROWNOUT";
        default:
            return "OTHER";
    }
}

static bool i2c_probe_7bit_addr(uint8_t addr)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return false;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK);
}

static bool oled_link_self_test(void)
{
    bool addr_ok = i2c_probe_7bit_addr(oled_get_i2c_addr());
    if (!addr_ok) {
        ESP_LOGW(TAG, "OLED probe failed at 0x%02X", oled_get_i2c_addr());
        return false;
    }

    OLED.init();
    OLED.clear();
    OLED.show_string(0, 0, "OLED TEST OK", 8);
    OLED.show_string(0, 16, "OV2640 INIT...", 8);
    OLED.refresh();
    ESP_LOGI(TAG, "OLED self-test passed");
    return true;
}

static void xcam_uart_init(void)
{
    ESP_ERROR_CHECK(uart_set_baudrate(XCAM_UART_PORT, XCAM_UART_BAUD));
    ESP_ERROR_CHECK(uart_set_pin(XCAM_UART_PORT,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));

    esp_err_t ret = uart_driver_install(XCAM_UART_PORT, XCAM_UART_TX_BUF_SIZE, 0, 0, NULL, 0);
    if (ret == ESP_FAIL || ret == ESP_ERR_INVALID_STATE) {
        uart_driver_delete(XCAM_UART_PORT);
        ret = uart_driver_install(XCAM_UART_PORT, XCAM_UART_TX_BUF_SIZE, 0, 0, NULL, 0);
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "XCAM UART ready on UART%d @ %d bps", (int)XCAM_UART_PORT, XCAM_UART_BAUD);
}

static bool jpeg_frame_is_valid(const camera_fb_t *fb)
{
    if (fb == NULL || fb->buf == NULL || fb->len < 4) {
        return false;
    }

    return (fb->buf[0] == 0xFF && fb->buf[1] == 0xD8 &&
            fb->buf[fb->len - 2] == 0xFF && fb->buf[fb->len - 1] == 0xD9);
}

static uint32_t frame_quick_signature(const camera_fb_t *fb)
{
    if (fb == NULL || fb->buf == NULL || fb->len == 0) {
        return 0;
    }

    uint32_t sig = 2166136261u;
    size_t step = fb->len / 64;
    if (step == 0) {
        step = 1;
    }

    for (size_t i = 0; i < fb->len; i += step) {
        sig ^= fb->buf[i];
        sig *= 16777619u;
    }
    sig ^= (uint32_t)fb->len;
    return sig;
}

static bool camera_link_self_test(void)
{
    const int frame_target = 5;
    int valid_frames = 0;
    int changed_frames = 0;
    uint32_t last_sig = 0;

    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL) {
        ESP_LOGE(TAG, "Camera self-test failed: sensor handle is NULL (SCCB not ready)");
        return false;
    }

    ESP_LOGI(TAG,
             "SCCB verify: MID=0x%02X%02X PID=0x%02X VER=0x%02X",
             s->id.MIDH,
             s->id.MIDL,
             s->id.PID,
             s->id.VER);

    for (int i = 0; i < frame_target; i++) {
        camera_fb_t *fb = CarCamera.get_frame();
        if (fb == NULL) {
            ESP_LOGE(TAG, "Camera self-test frame[%d]: fb is NULL", i);
            continue;
        }

        bool jpeg_ok = jpeg_frame_is_valid(fb);
        uint32_t sig = frame_quick_signature(fb);
        if (jpeg_ok) {
            valid_frames++;
        }
        if (i > 0 && sig != last_sig) {
            changed_frames++;
        }

        ESP_LOGI(TAG,
                 "Camera self-test frame[%d]: len=%u w=%u h=%u fmt=%d jpeg=%s sig=0x%08lX",
                 i,
                 (unsigned int)fb->len,
                 (unsigned int)fb->width,
                 (unsigned int)fb->height,
                 (int)fb->format,
                 jpeg_ok ? "yes" : "no",
                 (unsigned long)sig);

        last_sig = sig;
        CarCamera.return_frame(fb);
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    if (valid_frames < frame_target) {
        ESP_LOGE(TAG,
                 "Camera self-test failed: valid JPEG frames %d/%d",
                 valid_frames,
                 frame_target);
        return false;
    }

    if (changed_frames == 0) {
        ESP_LOGW(TAG,
                 "Camera self-test warning: frame signature unchanged across samples");
    }

    ESP_LOGI(TAG,
             "Camera self-test passed: valid=%d/%d, changed=%d/%d",
             valid_frames,
             frame_target,
             changed_frames,
             frame_target - 1);
    return true;
}

static esp_err_t uart_send_jpeg_frame(const camera_fb_t *fb)
{
    int written = uart_write_bytes(XCAM_UART_PORT, (const char *)fb->buf, fb->len);
    if (written != (int)fb->len) {
        ESP_LOGW(TAG, "uart frame write short: %d/%u", written, (unsigned int)fb->len);
        return ESP_FAIL;
    }

    esp_err_t ret = uart_wait_tx_done(XCAM_UART_PORT, pdMS_TO_TICKS(XCAM_UART_WRITE_TIMEOUT_MS));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "uart tx timeout: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

/*
    * @name   app_main
    * @brief  OLED + OV2640 bring-up test and serial JPG stream for XCAM host tool
    * @param  None
    * @retval None
*/
void app_main(void)
{
    ESP_LOGI(TAG, "OLED + OV2640 UART stream test start");
    ESP_LOGW(TAG, "Reset reason: %s", reset_reason_to_str(esp_reset_reason()));

    I2C_Master.init();
    bool oled_ok = oled_link_self_test();
    if (!oled_ok) {
        ESP_LOGW(TAG, "OLED self-test failed, continue with camera test");
    }

    // esp32-camera reuses SCCB pins; release legacy I2C driver to avoid bus ownership conflict.
    i2c_driver_delete(I2C_PORT_NUM);

    CarCamera.init();
    bool cam_ok = camera_link_self_test();
    if (!cam_ok) {
        if (oled_ok) {
            I2C_Master.init();
            OLED.clear();
            OLED.show_string(0, 0, "CAMERA TEST FAIL", 8);
            OLED.refresh();
        }
        ESP_LOGE(TAG, "Camera bring-up check failed, abort streaming");
        vTaskDelay(pdMS_TO_TICKS(1000));
        return;
    }

    if (oled_ok) {
        I2C_Master.init();
        OLED.clear();
        OLED.show_string(0, 0, "OLED OK", 8);
        OLED.show_string(0, 16, "OV2640 OK", 8);
        OLED.show_string(0, 32, "UART->XCAM", 8);
        OLED.refresh();
        i2c_driver_delete(I2C_PORT_NUM);
    }

    xcam_uart_init();
    ESP_LOGI(TAG, "Streaming JPEG over UART in raw SOI/EOI mode...");

    uint32_t frame_count = 0;
    while (1) {
        camera_fb_t *fb = CarCamera.get_frame();
        if (fb == NULL) {
            ESP_LOGW(TAG, "camera frame is NULL");
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (!jpeg_frame_is_valid(fb)) {
            ESP_LOGW(TAG, "invalid JPEG frame, len=%u", (unsigned int)fb->len);
            CarCamera.return_frame(fb);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        esp_err_t ret = uart_send_jpeg_frame(fb);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "uart send frame failed");
        }

        frame_count++;
        if ((frame_count % 20U) == 0U) {
            ESP_LOGI(TAG, "stream frame_count=%lu, last_len=%u",
                     (unsigned long)frame_count,
                     (unsigned int)fb->len);
        }

        CarCamera.return_frame(fb);
        vTaskDelay(pdMS_TO_TICKS(STREAM_FRAME_INTERVAL_MS));
    }
}
/********************************************************
  End Of File
********************************************************/
