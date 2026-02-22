#include "MyApplication.h"
#include "led_pwm.h"
#include "lcd.h"
#include "motor_driver.h"
#include "encoder.h"
#include "ov2640.h"
#include "uart.h"
#include <stdio.h>

// LED测试任务
void led_test_task(void *pvParameters) {
    while (1) {
        // 渐亮 (0 到 8191, 5秒)
        LED.led_fade_brightness(LED0, 0, 8191, 5000);
        // 渐暗 (8191 到 0, 5秒)
        LED.led_fade_brightness(LED0, 8191, 0, 5000);
    }
}

// TFT测试任务
void tft_test_task(void *pvParameters) {
    // printf("TFT_TEST: Starting TFT test\n");

    // 填充屏幕为白色
    TFT.tft_fill_screen(TFT_WHITE);
    // printf("TFT_TEST: Filled screen white\n");
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // 填充屏幕为红色
    TFT.tft_fill_screen(TFT_RED);
    // printf("TFT_TEST: Filled screen red\n");
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // 绘制蓝色矩形
    TFT.tft_draw_rectangle(20, 20, 50, 50, TFT_BLUE);
    // printf("TFT_TEST: Drew blue rectangle\n");
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // 绘制绿色像素
    for (int i = 0; i < 10; i++) {
        TFT.tft_draw_pixel(60 + i, 60 + i, TFT_GREEN);
    }
    // printf("TFT_TEST: Drew green pixels\n");
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // 关闭背光
    TFT.tft_backlight_off();
    // printf("TFT_TEST: Backlight off\n");
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // 重新开启背光
    TFT.tft_backlight_on();
    // printf("TFT_TEST: Backlight on\n");

    // 任务结束
    vTaskDelete(NULL);
}

// OV2640测试任务
void ov2640_test_task(void *pvParameters) {
    // printf("OV2640_TEST: Starting OV2640 test\n");

    while (1) {
        camera_fb_t* fb = OV2640.ov2640_capture_frame();
        if (fb) {
            OV2640.ov2640_display_to_tft(fb);
            OV2640.ov2640_free_frame(fb);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);  // ~10fps, adjust for framerate
    }
}

// 电机测试任务
void motor_test_task(void *pvParameters) {
    // printf("MOTOR_TEST: Starting motor test\n");


    while(1) {
        // 正转5秒
        set_motor_speed(50, 1);  // 50%速度正转
        // printf("MOTOR_TEST: Motor forward\n");
        // vTaskDelay(5000 / portTICK_PERIOD_MS);
        
        // // 停止2秒
        // set_motor_speed(0, 0);
        // // printf("MOTOR_TEST: Motor stop\n");
        // vTaskDelay(2000 / portTICK_PERIOD_MS);
        
        // // 反转5秒
        // set_motor_speed(50, 2);  // 50%速度反转
        // // printf("MOTOR_TEST: Motor backward\n");
        // vTaskDelay(5000 / portTICK_PERIOD_MS);
        
        // // 停止2秒
        // set_motor_speed(0, 0);
        // // printf("MOTOR_TEST: Motor stop\n");
        // vTaskDelay(2000 / portTICK_PERIOD_MS);
        
        // 读取并显示编码器状态
        // encoder_status_t status = Encoder.encoder_get_status();
        // printf("ENCODER: Pulse count: %ld\n", status.pulse_count);
        // printf("ENCODER: RPM: %.2f\n", status.rpm);
        // printf("ENCODER: Rotations: %.2f\n", status.rotations);

        // 通过UART发送霍尔传感器数据
        char buffer[100];
        // sprintf(buffer, "Pulse count: %ld, RPM: %.2f, Rotations: %.2f\n", status.pulse_count, status.rpm, status.rotations);
        // uart0_send_string(buffer);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// 串口0发送接收任务
void uart0_send_receive_task(void *pvParameters) {

    const char *test_str = "Hello from UART0!\n";
    while (1) {
        // uart0_send_string(test_str);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


/**
 * @brief 主函数，系统入口点
 * 该函数负责初始化各种外设并创建相应的任务
 */
void app_main(void)
{
    // // 初始化LED
    // LED.led_init();

    // 初始化TFT
    TFT.tft_init();

    // 初始化OV2640
    OV2640.ov2640_init();

    // 初始化串口0
    uart0_init();

    // 初始化电机驱动与编码器
    motor_driver_init();
    Encoder.encoder_init();

    // 创建串口0发送接收任务
    xTaskCreate(uart0_send_receive_task, "UART0 Send/Receive", 4096, NULL, 0, NULL);

    // // 创建LED测试任务
    // xTaskCreate(led_test_task, "LED Test", 2048, NULL, 0, NULL);

    // 创建TFT测试任务
    xTaskCreate(tft_test_task, "TFT Test", 4096, NULL, 0, NULL);

    // 启动OV2640串口推流（每200 ms一帧）
    OV2640.ov2640_start_uart_stream(200);
    
    // 创建电机测试任务
    xTaskCreate(motor_test_task, "Motor Test", 4096, NULL, 0, NULL);
}
