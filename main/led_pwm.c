/* Includes ------------------------------------------------------------------*/
#include "MyApplication.h"
#include "led_pwm.h"

/* Private define-------------------------------------------------------------*/
#define LEDC_CHANNEL_0 LEDC_CHANNEL_0
#define LEDC_CHANNEL_1 LEDC_CHANNEL_1
#define MAX_DUTY ((1 << LEDC_DUTY_RES) - 1) // 8191 for 13-bit

/* Private variables----------------------------------------------------------*/
static void led_init();
static void led_on(uint32_t gpio_num);
static void led_off(uint32_t gpio_num);
static void led_flip(uint32_t gpio_num);
static void led_set_brightness(uint32_t gpio_num, uint32_t brightness);
static void led_fade_brightness(uint32_t gpio_num, uint32_t start_duty, uint32_t end_duty, uint32_t fade_time_ms);

/* Public variables-----------------------------------------------------------*/
LED_t  LED =
{
	led_init,
    led_on,
    led_off,
    led_flip,
    led_set_brightness,
    led_fade_brightness,
};

/* Private variables for state */
static int led0_brightness = 0;
static int led1_brightness = 0;

/* Private function prototypes------------------------------------------------*/
/**
 * @brief 初始化LED控制相关的LEDC定时器和通道
 *        该函数配置了LEDC定时器，安装了渐变功能，并初始化了两个LED通道
 *        最后将两个LED默认设置为关闭状态
 */
static void led_init()
{
    // 配置LEDC定时器参数，包括模式、定时器编号、分辨率、频率和时钟配置
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,      // LEDC工作模式
        .timer_num = LEDC_TIMER,      // 定时器编号
        .duty_resolution = LEDC_DUTY_RES,  // 占空比分辨率
        .freq_hz = LEDC_FREQUENCY,    // 输出频率(Hz)
        .clk_cfg = LEDC_AUTO_CLK      // 时钟配置
    };
    ledc_timer_config(&ledc_timer);   // 应用定时器配置

    // 安装LEDC fade功能
    ledc_fade_func_install(0);

    // 配置LEDC通道0 for LED0
    ledc_channel_config_t ledc_channel0 = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_FADE_END,  // 启用fade中断
        .gpio_num = LED0,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel0);

    // 配置LEDC通道1 for LED1
    ledc_channel_config_t ledc_channel1 = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_FADE_END,  // 启用fade中断
        .gpio_num = LED1,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel1);

    // 默认关闭LED0和LED1
    led_set_brightness(LED0, 0);
    led_set_brightness(LED1, 0);
}

static void led_on(uint32_t gpio_num)
{
    led_set_brightness(gpio_num, 255);
}

static void led_off(uint32_t gpio_num)
{
    led_set_brightness(gpio_num, 0);
}

static void led_flip(uint32_t gpio_num)
{
    if (gpio_num == LED0) {
        if (led0_brightness == 0) {
            led_set_brightness(LED0, 255);
            led0_brightness = 1;
        } else {
            led_set_brightness(LED0, 0);
            led0_brightness = 0;
        }
    } else if (gpio_num == LED1) {
        if (led1_brightness == 0) {
            led_set_brightness(LED1, 255);
            led1_brightness = 1;
        } else {
            led_set_brightness(LED1, 0);
            led1_brightness = 0;
        }
    }
}

static void led_set_brightness(uint32_t gpio_num, uint32_t brightness)
{
    if (brightness > 255) brightness = 255;
    uint32_t duty = (brightness * MAX_DUTY) / 255;

    if (gpio_num == LED0) {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
        led0_brightness = brightness;
    } else if (gpio_num == LED1) {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
        led1_brightness = brightness;
    }
}

int led_get_brightness(uint32_t gpio_num) {
    if (gpio_num == LED0) return led0_brightness;
    if (gpio_num == LED1) return led1_brightness;
    return 0;
}

static void led_fade_brightness(uint32_t gpio_num, uint32_t start_duty, uint32_t end_duty, uint32_t fade_time_ms) {
    ledc_channel_t channel = (gpio_num == LED0) ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1;
    // 设置起始占空比
    ledc_set_duty(LEDC_MODE, channel, start_duty);
    ledc_update_duty(LEDC_MODE, channel);
    // 配置渐变到目标占空比
    ledc_set_fade_with_time(LEDC_MODE, channel, end_duty, fade_time_ms);
    // 开始渐变，阻塞等待完成
    ledc_fade_start(LEDC_MODE, channel, LEDC_FADE_WAIT_DONE);
    // 更新亮度状态（近似）
    if (gpio_num == LED0) led0_brightness = (end_duty * 255) / MAX_DUTY;
    else if (gpio_num == LED1) led1_brightness = (end_duty * 255) / MAX_DUTY;
}

