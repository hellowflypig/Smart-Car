/* Includes ------------------------------------------------------------------*/
#include "servo.h"
#include "driver/ledc.h"

/* Private define-------------------------------------------------------------*/
#define SERVO_LEDC_TIMER        LEDC_TIMER_1
#define SERVO_LEDC_MODE         LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_CHANNEL      LEDC_CHANNEL_4 // Channel 0-3 used by motors
#define SERVO_LEDC_DUTY_RES     LEDC_TIMER_13_BIT // 13 bit resolution (8192)
#define SERVO_LEDC_FREQUENCY    50 // 50 Hz for SG90 servo

// Calculate duty values for 13-bit resolution (8192) at 50Hz (20ms period)
// 0.5ms (0 degrees) = (0.5 / 20.0) * 8192 ≈ 205
// 2.5ms (180 degrees) = (2.5 / 20.0) * 8192 ≈ 1024
#define SERVO_MIN_PULSEWIDTH    205 
#define SERVO_MAX_PULSEWIDTH    1024

/* Private variables----------------------------------------------------------*/
static const char *TAG = "SERVO";

/* Private function prototypes------------------------------------------------*/
static void servo_init(void);
static void servo_set_angle(float angle);

/* Public variables-----------------------------------------------------------*/
Servo_t CameraServo = {
    servo_init,
    servo_set_angle
};

/* Private function definitions-----------------------------------------------*/

/*
    * @name   servo_init
    * @brief  Initialize LEDC for SG90 Servo
    * @param  None
    * @retval None      
*/
static void servo_init(void)
{
    ESP_LOGI(TAG, "Initializing Camera Servo...");
    
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = SERVO_LEDC_MODE,
        .timer_num        = SERVO_LEDC_TIMER,
        .duty_resolution  = SERVO_LEDC_DUTY_RES,
        .freq_hz          = SERVO_LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = SERVO_LEDC_MODE,
        .channel        = SERVO_LEDC_CHANNEL,
        .timer_sel      = SERVO_LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = SERVO_PIN,
        .duty           = SERVO_MIN_PULSEWIDTH + ((SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) / 2), // 90 degree default
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

/*
    * @name   servo_set_angle
    * @brief  Set Servo Angle (0-180)
    * @param  angle: float
    * @retval None      
*/
static void servo_set_angle(float angle)
{
    if(angle < 0.0f) angle = 0.0f;
    if(angle > 180.0f) angle = 180.0f;
    
    uint32_t duty = SERVO_MIN_PULSEWIDTH + (uint32_t)(((SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * angle) / 180.0f);
    
    ledc_set_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL, duty);
    ledc_update_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL);
}

/********************************************************
  End Of File
********************************************************/