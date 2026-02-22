/* Includes ------------------------------------------------------------------*/
#include "MyApplication.h"
#include "encoder.h"

/* Private variables----------------------------------------------------------*/
static const gpio_num_t encoder_a_gpios[4] = {ENCODER_A1_GPIO, ENCODER_A2_GPIO, ENCODER_A3_GPIO, ENCODER_A4_GPIO};
static const gpio_num_t encoder_b_gpios[4] = {ENCODER_B1_GPIO, ENCODER_B2_GPIO, ENCODER_B3_GPIO, ENCODER_B4_GPIO};
static const pcnt_unit_t encoder_pcnt_units[4] = {PCNT_UNIT_LF, PCNT_UNIT_LB, PCNT_UNIT_RF, PCNT_UNIT_RB};

static encoder_status_t encoder_status = {0};

/* Private function prototypes------------------------------------------------*/
static void encoder_init();
static encoder_status_t encoder_get_status();

/* Public variables-----------------------------------------------------------*/
Encoder_t Encoder =
{
    encoder_init,
    encoder_get_status,
};

/* Public functions-----------------------------------------------------------*/
static void encoder_init() {
    // 配置所有编码器 GPIO（启用上拉）
    uint64_t pin_mask = 0;
    for (int i = 0; i < 4; ++i) {
        pin_mask |= (1ULL << encoder_a_gpios[i]);
        pin_mask |= (1ULL << encoder_b_gpios[i]);
    }

    gpio_config_t encoder_io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&encoder_io_conf);

    // 为每个编码器单元配置 PCNT
    for (int i = 0; i < 4; ++i) {
        pcnt_config_t pcnt_cfg = {
            .pulse_gpio_num = encoder_a_gpios[i],
            .ctrl_gpio_num = encoder_b_gpios[i],
            .lctrl_mode = PCNT_MODE_REVERSE,
            .hctrl_mode = PCNT_MODE_KEEP,
            .pos_mode = PCNT_COUNT_INC,
            .neg_mode = PCNT_COUNT_DEC,
            .counter_h_lim = 10000,
            .counter_l_lim = -10000,
            .unit = encoder_pcnt_units[i],
            .channel = PCNT_CHANNEL_0,
        };

        ESP_ERROR_CHECK(pcnt_unit_config(&pcnt_cfg));
        ESP_ERROR_CHECK(pcnt_counter_clear(encoder_pcnt_units[i]));
        ESP_ERROR_CHECK(pcnt_counter_resume(encoder_pcnt_units[i]));
    }
}

static encoder_status_t encoder_get_status() {
    int32_t total_count = 0;
    float total_rpm = 0.0f;
    float total_rot = 0.0f;
    int valid_units = 0;

    for (int i = 0; i < 4; ++i) {
        int16_t cnt = 0;
        esp_err_t err = pcnt_get_counter_value(encoder_pcnt_units[i], &cnt);
        if (err == ESP_OK) {
            float rpm = (cnt * 60.0f) / (float)ENCODER_PULSES_PER_REV;
            float rotations = (float)cnt / (float)ENCODER_PULSES_PER_REV;
            total_count += cnt;
            total_rpm += rpm;
            total_rot += rotations;
            valid_units++;
        }

        // 清零每个计数器，为下次采样准备
        pcnt_counter_clear(encoder_pcnt_units[i]);
    }

    if (valid_units > 0) {
        encoder_status.pulse_count = total_count;
        encoder_status.rpm = total_rpm / (float)valid_units;        // 平均 rpm
        encoder_status.rotations = total_rot / (float)valid_units;  // 平均圈数
    }

    return encoder_status;
}
