#/********************************************************************************
 * motor_driver.c
 *
 * 功能：
 *   - 为基于 TB6612FNG1 驱动芯片的四轮驱动电机提供初始化与统一控制接口。
 *   - 使用 LEDC（PWM）输出控制四轮电机的占空比（速度），使用 GPIO 控制驱动芯片的方向和待机（STBY）引脚。
 *
 * 设计说明：
 *   - 本文件按“每对车轮共享 PWM 通道、独立方向引脚”的方式实现：
 *       LF (左前) 使用 AIN1/AIN2 与 PWMA_CHANNEL_LF/PWMA_GPIO_LF
 *       LB (左后) 使用 BIN1/BIN2 与 PWMB_CHANNEL_LB/PWMB_GPIO_LB
 *       RF (右前) 使用 AIN1/AIN2 与 PWMA_CHANNEL_RF/PWMA_GPIO_RF
 *       RB (右后) 使用 BIN1/BIN2 与 PWMB_CHANNEL_RB/PWMB_GPIO_RB
 *     注：若需每轮独立 PWM 或方向引脚，请在头文件中提供各轮独立定义并调整实现。
 *
 *   - PWM：使用 LEDC 的一个定时器（MOTOR_PWM_TIMER）和 4 个通道来输出 PWM 信号。
 *     占空比转换由 `speed_to_duty()` 完成（将 0-100% 映射到 LEDC 分辨率范围）。
 *
 * 导出接口：
 *   - void motor_driver_init(void): 初始化方向 GPIO、PWM 定时器与通道，并解除 STBY。
 *   - void set_motor_speed(uint8_t speed, uint8_t direction): 统一设置四轮速度与方向。
 *       direction: 0=停止, 1=正转, 2=反转
 *
 * 注意事项：
 *   - 该实现对四轮统一设置同一速度与方向；若需要独立控制每轮，请扩展接口以接受每轮参数。
 *   - 改变方向时建议先把 PWM 置 0 再切换方向，以避免电流冲击。
 *
 ********************************************************************************/

#include "motor_driver.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

// PWM 分辨率与最大占空比
#define PWM_RESOLUTION    LEDC_TIMER_13_BIT
#define PWM_MAX_DUTY      ((1 << 13) - 1)

/**
 * configure_direction_pins
 * ------------------------
 * 将所有与电机方向相关的 GPIO 配置为推挽输出：
 *   - AIN1/AIN2 (左前与右前方向控制)
 *   - BIN1/BIN2 (左后与右后方向控制)
 *   - STBY (驱动芯片待机控制)
 *
 * 说明：TB6612FNG 的方向由两位 GPIO 控制（例如 AIN1=1, AIN2=0 表示正转），
 * 因此仅需在运行时将相应 GPIO 置 0/1 即可实现前进/后退。
 */
static void configure_direction_pins(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MOTOR_AIN1_GPIO_LF) | (1ULL << MOTOR_AIN2_GPIO_LF) |
                        (1ULL << MOTOR_BIN1_GPIO_LB) | (1ULL << MOTOR_BIN2_GPIO_LB) |
                        (1ULL << MOTOR_AIN1_GPIO_RF) | (1ULL << MOTOR_AIN2_GPIO_RF) |
                        (1ULL << MOTOR_BIN1_GPIO_RB) | (1ULL << MOTOR_BIN2_GPIO_RB) |
                        (1ULL << MOTOR_STBY_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

/**
 * configure_pwm
 * -------------
 * 配置 LEDC 定时器与通道，用于生成 PWM 信号以驱动电机的 PWMA/PWMB 引脚。
 *
 * 关键参数说明：
 *   - speed_mode: 选用 LEDC_LOW_SPEED_MODE，以确保与系统时钟和硬件兼容性。
 *   - duty_resolution: 由 PWM_RESOLUTION 决定（本例为 13-bit），决定占空比分辨率。
 *   - timer_num: 使用头文件中定义的 MOTOR_PWM_TIMER（所有使用该 timer 的通道应一致）。
 *   - freq_hz: PWM 频率，这里示例为 5kHz，可按电机与驱动要求调整（注意噪声与切换频率）。
 *
 * 本函数会为四个需要 PWM 的 GPIO 分别注册 LEDC 通道，并将初始占空比置为 0。
 */
static void configure_pwm(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = PWM_RESOLUTION,
        .timer_num = MOTOR_PWM_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    /* 为每个需要输出 PWM 的 GPIO 分配 LEDC 通道并初始化占空比为 0 */
    ledc_channel_config_t ch_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = MOTOR_PWMA_CHANNEL_LF,
        .timer_sel = MOTOR_PWM_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = MOTOR_PWMA_GPIO_LF,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));

    ch_cfg.channel = MOTOR_PWMB_CHANNEL_LB;
    ch_cfg.gpio_num = MOTOR_PWMB_GPIO_LB;
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));

    ch_cfg.channel = MOTOR_PWMA_CHANNEL_RF;
    ch_cfg.gpio_num = MOTOR_PWMA_GPIO_RF;
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));

    ch_cfg.channel = MOTOR_PWMB_CHANNEL_RB;
    ch_cfg.gpio_num = MOTOR_PWMB_GPIO_RB;
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
}

/**
 * motor_driver_init
 * -----------------
 * 初始化电机驱动相关的硬件资源：
 *   1. 配置方向 GPIO（输出）
 *   2. 配置 LEDC 定时器与 PWM 通道
 *   3. 将 STBY 引脚置高以解除待机，允许驱动芯片输出
 *
 * 注意：调用此函数前应确保相关 GPIO 编号在头文件中配置正确。
 */
void motor_driver_init() {
    configure_direction_pins();
    configure_pwm();

    // 解除 STBY，允许驱动（高电平表示工作）
    gpio_set_level(MOTOR_STBY_GPIO, 1);
}

/**
 * speed_to_duty
 * -------------
 * 将用户级速度百分比（0-100）映射为 LEDC 所需的占空比值（基于 PWM_RESOLUTION）。
 * 例如：当 PWM_RESOLUTION = 13 时，PWM_MAX_DUTY = 8191；50% -> ~4095
 */
static inline uint32_t speed_to_duty(uint8_t speed_percent) {
    if (speed_percent > 100) speed_percent = 100;
    return (uint32_t)((speed_percent * (uint32_t)PWM_MAX_DUTY) / 100);
}

/**
 * set_motor_speed
 * ---------------
 * 统一设置四轮电机的速度与方向。
 * 参数：
 *   - speed: 0-100 (%)，表示目标速度的百分比。
 *   - direction: 0=停止, 1=正转, 2=反转。
 *
 * 逻辑说明：
 *   - 若 direction == 0（停止），则把所有方向引脚拉低，且将各通道 PWM 占空比设为 0，保证电机停止。
 *   - 若 direction == 1（正转），则为 A 型电机（LF, RF）设置 AIN1=1,AIN2=0；为 B 型（LB, RB）设置 BIN1=1,BIN2=0。
 *     direction == 2 时取相反电平以实现反转。
 *   - 同时将计算得到的占空比写入对应的 LEDC 通道，控制速度。
 *
 * 安全注意：
 *   - 在实际硬件上，改变方向时可能需要短暂先将 PWM 置 0 再切换方向以避免电流冲击。
 *   - 若需要闭环控制或匀速控制，请基于编码器反馈实现 PID 控制器，而不是直接使用此开环接口。
 */
void set_motor_speed(uint8_t speed, uint8_t direction) {
    uint32_t duty = speed_to_duty(speed);

    /* 停止情况：将方向全部拉低，并把 PWM 置 0 */
    if (direction == 0) {
        gpio_set_level(MOTOR_AIN1_GPIO_LF, 0);
        gpio_set_level(MOTOR_AIN2_GPIO_LF, 0);
        gpio_set_level(MOTOR_BIN1_GPIO_LB, 0);
        gpio_set_level(MOTOR_BIN2_GPIO_LB, 0);
        gpio_set_level(MOTOR_AIN1_GPIO_RF, 0);
        gpio_set_level(MOTOR_AIN2_GPIO_RF, 0);
        gpio_set_level(MOTOR_BIN1_GPIO_RB, 0);
        gpio_set_level(MOTOR_BIN2_GPIO_RB, 0);

        ledc_set_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMA_CHANNEL_LF, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMA_CHANNEL_LF);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMB_CHANNEL_LB, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMB_CHANNEL_LB);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMA_CHANNEL_RF, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMA_CHANNEL_RF);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMB_CHANNEL_RB, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMB_CHANNEL_RB);

        return;
    }

    /* 正转/反转：根据 direction 设置方向级别 */
    int a1_level = (direction == 1) ? 1 : 0;
    int a2_level = (direction == 1) ? 0 : 1;
    int b1_level = a1_level;
    int b2_level = a2_level;

    gpio_set_level(MOTOR_AIN1_GPIO_LF, a1_level);
    gpio_set_level(MOTOR_AIN2_GPIO_LF, a2_level);
    gpio_set_level(MOTOR_BIN1_GPIO_LB, b1_level);
    gpio_set_level(MOTOR_BIN2_GPIO_LB, b2_level);
    gpio_set_level(MOTOR_AIN1_GPIO_RF, a1_level);
    gpio_set_level(MOTOR_AIN2_GPIO_RF, a2_level);
    gpio_set_level(MOTOR_BIN1_GPIO_RB, b1_level);
    gpio_set_level(MOTOR_BIN2_GPIO_RB, b2_level);

    /* 写入 PWM 占空比，控制速度 */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMA_CHANNEL_LF, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMA_CHANNEL_LF);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMB_CHANNEL_LB, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMB_CHANNEL_LB);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMA_CHANNEL_RF, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMA_CHANNEL_RF);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMB_CHANNEL_RB, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, MOTOR_PWMB_CHANNEL_RB);
}
