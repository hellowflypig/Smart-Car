/* Includes ------------------------------------------------------------------*/
#include "motor.h"
#include "driver/ledc.h"
#include "driver/pulse_cnt.h"
#include "esp_timer.h"

/* Private define-------------------------------------------------------------*/
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_10_BIT // 10 bit, 0-1023
#define LEDC_FREQUENCY          20000 // 20 kHz PWM for motors

#define PCNT_HIGH_LIMIT         10000
#define PCNT_LOW_LIMIT          -10000

#define PID_INTERVAL_US         10000 // 10ms timer for PID

/* Struct---------------------------------------------------------------------*/
typedef struct {
    float kp, ki, kd;
    float error, last_error;
    float integral, max_integral;
    float output, max_output;
} PID_Controller_t;

/* Private variables----------------------------------------------------------*/
static pcnt_unit_handle_t pcnt_units[4];
static esp_timer_handle_t pid_timer;
static const char *TAG = "MOTOR";

// 记录速度和位置
static int32_t motor_position[4] = {0};
static int32_t motor_speed[4] = {0};

// 目标速度和位置
static int32_t target_position[4] = {0};
static int32_t target_speed[4] = {0};
static bool open_loop_mode = false;

// 双环PID控制器
static PID_Controller_t pos_pid[4];
static PID_Controller_t speed_pid[4];

/* Private function prototypes------------------------------------------------*/
static void motor_init(void);
static void motor_set_speed(Motor_ID_t id, int speed);
static void motor_set_target(Motor_ID_t id, int32_t target_pos, int32_t target_speed);
static void motor_update_pid(void *arg);
static void motor_apply_speed(Motor_ID_t id, int speed, bool set_open_loop);
static void motor_poll_feedback(void);

/* Public variables-----------------------------------------------------------*/
Motor_t CarMotor = {
    motor_init,
    motor_set_speed,
    motor_set_target
};

/* Private function definitions-----------------------------------------------*/

/*
    * @name   pid_compute
    * @brief  Calculate PID output
*/
static float pid_compute(PID_Controller_t *pid, float target, float current)
{
    pid->error = target - current;
    pid->integral += pid->error;
    
    // Integral anti-windup
    if(pid->integral > pid->max_integral) pid->integral = pid->max_integral;
    if(pid->integral < -pid->max_integral) pid->integral = -pid->max_integral;
    
    pid->output = (pid->kp * pid->error) + (pid->ki * pid->integral) + (pid->kd * (pid->error - pid->last_error));
    pid->last_error = pid->error;
    
    // Output limit
    if(pid->output > pid->max_output) pid->output = pid->max_output;
    if(pid->output < -pid->max_output) pid->output = -pid->max_output;
    
    return pid->output;
}

/*
    * @name   pid_init
    * @brief  Initialize PID parameters
*/
static void pid_init(void)
{
    for(int i=0; i<4; i++) {
        // Position PID init
        pos_pid[i].kp = 0.5f; 
        pos_pid[i].ki = 0.0f;
        pos_pid[i].kd = 0.1f;
        pos_pid[i].max_integral = 1000.0f;
        pos_pid[i].max_output = 50.0f; // Max target speed from pos loop
        
        // Speed PID init
        speed_pid[i].kp = 15.0f; 
        speed_pid[i].ki = 1.0f;
        speed_pid[i].kd = 0.5f;
        speed_pid[i].max_integral = 5000.0f;
        speed_pid[i].max_output = 1023.0f; // Max PWM duty cycle
    }
}

/*
    * @name   encoder_init
    * @brief  Initialize PCNT for 4 encoders
    * @param  None
    * @retval None      
*/
static void encoder_init(void)
{
    // 配置4个编码器单元的管脚
    int enc_pins[4][2] = {
        {MOTOR_LF_ENC_A, MOTOR_LF_ENC_B}, // LF
        {MOTOR_LR_ENC_A, MOTOR_LR_ENC_B}, // LR
        {MOTOR_RF_ENC_A, MOTOR_RF_ENC_B}, // RF
        {MOTOR_RR_ENC_A, MOTOR_RR_ENC_B}  // RR
    };

    pcnt_unit_config_t unit_config = {
        .high_limit = PCNT_HIGH_LIMIT,
        .low_limit = PCNT_LOW_LIMIT,
    };

    for(int i=0; i<4; i++) {
        pcnt_new_unit(&unit_config, &pcnt_units[i]);

        pcnt_glitch_filter_config_t filter_config = {
            .max_glitch_ns = 1000,
        };
        pcnt_unit_set_glitch_filter(pcnt_units[i], &filter_config);

        pcnt_chan_config_t chan_a_config = {
            .edge_gpio_num = enc_pins[i][0],
            .level_gpio_num = enc_pins[i][1],
        };
        pcnt_channel_handle_t pcnt_chan_a = NULL;
        pcnt_new_channel(pcnt_units[i], &chan_a_config, &pcnt_chan_a);

        pcnt_chan_config_t chan_b_config = {
            .edge_gpio_num = enc_pins[i][1],
            .level_gpio_num = enc_pins[i][0],
        };
        pcnt_channel_handle_t pcnt_chan_b = NULL;
        pcnt_new_channel(pcnt_units[i], &chan_b_config, &pcnt_chan_b);

        pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
        pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

        pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
        pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

        pcnt_unit_enable(pcnt_units[i]);
        pcnt_unit_clear_count(pcnt_units[i]);
        pcnt_unit_start(pcnt_units[i]);
    }
}

/*
    * @name   pwm_init
    * @brief  Initialize LEDC for Motor PWMs
    * @param  None
    * @retval None      
*/
static void pwm_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    int pwm_pins[4] = {MOTOR_L_PWMA, MOTOR_L_PWMB, MOTOR_R_PWMA, MOTOR_R_PWMB};
    
    for(int i=0; i<4; i++) {
        ledc_channel_config_t ledc_channel = {
            .speed_mode     = LEDC_MODE,
            .channel        = (ledc_channel_t)i,
            .timer_sel      = LEDC_TIMER,
            .intr_type      = LEDC_INTR_DISABLE,
            .gpio_num       = pwm_pins[i],
            .duty           = 0, 
            .hpoint         = 0
        };
        ledc_channel_config(&ledc_channel);
    }
}

/*
    * @name   motor_init
    * @brief  Motor Initialize (PWM, PCNT for encoder, PID Timer)
    * @param  None
    * @retval None      
*/
static void motor_init(void)
{
    ESP_LOGI(TAG, "Initializing Motors and Encoders...");
    pwm_init();
    encoder_init();
    pid_init();
    
    // Create and start periodic timer for PID computation
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &motor_update_pid,
        .name = "pid_loop"
    };
    esp_timer_create(&periodic_timer_args, &pid_timer);
    esp_timer_start_periodic(pid_timer, PID_INTERVAL_US);
    
    // Keep STBY low after power-on; app_main will enable it only during motion command.
    PCA_Extender.set_pin(PCA_PIN_0_6, 0);
    
    ESP_LOGI(TAG, "Motor Init Complete");
}

/*
    * @name   motor_set_target
    * @brief  Set Motor Target Position and Speed
*/
static void motor_set_target(Motor_ID_t id, int32_t target_pos, int32_t target_spd)
{
    open_loop_mode = false;
    target_position[id] = target_pos;
    target_speed[id] = target_spd;
}

/*
    * @name   motor_set_speed
    * @brief  Set Motor PWM output (-1023 to 1023)
    * @param  id: Motor_ID_t, speed: int
    * @retval None      
*/
static void motor_set_speed(Motor_ID_t id, int speed)
{
    motor_apply_speed(id, speed, true);
}

static void motor_apply_speed(Motor_ID_t id, int speed, bool set_open_loop)
{
    if (set_open_loop) {
        open_loop_mode = true;
    }

    uint32_t duty = abs(speed);
    if(duty > 1023) duty = 1023;
    
    uint8_t dir = (speed >= 0) ? 1 : 0;
    
    // Motor logic: AIN1/2, BIN1/2 on PCA9535
    // Port 1_0 to 1_7: LF_A1, LF_A2, LR_B1, LR_B2, RF_A1, RF_A2, RR_B1, RR_B2
    switch(id) {
        case MOTOR_LF: // PCA_PIN_1_0, PCA_PIN_1_1
            PCA_Extender.set_pin(PCA_PIN_1_0, dir);
            PCA_Extender.set_pin(PCA_PIN_1_1, !dir);
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, duty);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
            break;
        case MOTOR_LR: // PCA_PIN_1_2, PCA_PIN_1_3
            PCA_Extender.set_pin(PCA_PIN_1_2, dir);
            PCA_Extender.set_pin(PCA_PIN_1_3, !dir);
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, duty);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
            break;
        case MOTOR_RF: // PCA_PIN_1_4, PCA_PIN_1_5
            PCA_Extender.set_pin(PCA_PIN_1_4, dir);
            PCA_Extender.set_pin(PCA_PIN_1_5, !dir);
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, duty);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2);
            break;
        case MOTOR_RR: // PCA_PIN_1_6, PCA_PIN_1_7
            PCA_Extender.set_pin(PCA_PIN_1_6, dir);
            PCA_Extender.set_pin(PCA_PIN_1_7, !dir);
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_3, duty);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_3);
            break;
    }
}

/*
    * @name   motor_update_pid
    * @brief  Update PID calculations (run periodically by esp_timer)
    * @param  arg: Timer argument
    * @retval None      
*/
static void motor_update_pid(void *arg)
{
    motor_poll_feedback();

    if (open_loop_mode) {
        return;
    }

    // Closed-loop output update
    for(int i=0; i<4; i++) {
        // --- 1. Position Loop ---
        // Input: target_position, current_position
        // Output: desired target speed
        float pos_pid_out = pid_compute(&pos_pid[i], (float)target_position[i], (float)motor_position[i]);
        float spd_target = pos_pid_out;

        // --- 2. Speed Loop ---
        // Input: target_speed (from pos loop), actual_speed
        // Output: PWM duty cycle
        float spd_pid_out = pid_compute(&speed_pid[i], spd_target, (float)motor_speed[i]);

        // Apply final PWM to the motor without leaving closed-loop mode
        motor_apply_speed((Motor_ID_t)i, (int)spd_pid_out, false);
    }
}

static void motor_poll_feedback(void)
{
    // Read PCNT count for all 4 motors
    for(int i=0; i<4; i++) {
        int count = 0;
        pcnt_unit_get_count(pcnt_units[i], &count);
        pcnt_unit_clear_count(pcnt_units[i]);
        
        // Calculate speed (pulses per interval) and accumulate position
        motor_speed[i] = count;
        motor_position[i] += count;
        
    }
}

int32_t motor_get_position(Motor_ID_t id)
{
    if ((int)id < 0 || id > MOTOR_RR) {
        return 0;
    }
    return motor_position[id];
}

void motor_get_all_positions(int32_t pos_out[4])
{
    if (pos_out == NULL) {
        return;
    }

    for (int i = 0; i < 4; i++) {
        pos_out[i] = motor_position[i];
    }
}

/********************************************************
  End Of File
********************************************************/