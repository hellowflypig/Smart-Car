/* Includes ------------------------------------------------------------------*/
#include "pca9535.h"
#include "driver/i2c.h"

/* Private define-------------------------------------------------------------*/
#define REG_INPUT_PORT0     0x00
#define REG_OUTPUT_PORT0    0x02
#define REG_CONFIG_PORT0    0x06

/* Private variables----------------------------------------------------------*/
static const char *TAG = "PCA9535";
static uint16_t out_reg_cache = 0xFFFF; // Caching the output register state
static uint16_t config_reg_cache = 0x0000; // All output by default for motors
static uint8_t pca9535_addr = PCA9535_I2C_ADDR;
static bool pca9535_ready = false;

/* Private function prototypes------------------------------------------------*/
static void pca9535_init(void);
static void pca9535_set_pin(PCA_Pin_t pin, uint8_t level);
static uint8_t pca9535_get_pin(PCA_Pin_t pin);
static bool pca9535_detect_addr(void);

/* Public variables-----------------------------------------------------------*/
PCA9535_t PCA_Extender = {
    pca9535_init,
    pca9535_set_pin,
    pca9535_get_pin
};

/* Private function definitions-----------------------------------------------*/
static void pca9535_write_reg(uint8_t reg, uint16_t data)
{
    if (!pca9535_ready) {
        ESP_LOGE(TAG, "Device not ready, skip write reg 0x%02x", reg);
        return;
    }

    uint8_t write_buf[3] = {reg, (uint8_t)(data & 0xFF), (uint8_t)(data >> 8)};
    esp_err_t err = i2c_master_write_to_device(I2C_PORT_NUM, pca9535_addr, write_buf, 3, 1000 / portTICK_PERIOD_MS);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "I2C Write Failed to addr 0x%02x! err: %s (0x%x)", pca9535_addr, esp_err_to_name(err), err);
    }
}

static uint16_t pca9535_read_reg(uint8_t reg)
{
    if (!pca9535_ready) {
        ESP_LOGE(TAG, "Device not ready, skip read reg 0x%02x", reg);
        return 0;
    }

    uint8_t read_buf[2] = {0};
    esp_err_t err = i2c_master_write_read_device(I2C_PORT_NUM, pca9535_addr, &reg, 1, read_buf, 2, 1000 / portTICK_PERIOD_MS);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "I2C Read Failed from addr 0x%02x! err: %s (0x%x)", pca9535_addr, esp_err_to_name(err), err);
    }
    return (read_buf[1] << 8) | read_buf[0];
}

static bool pca9535_detect_addr(void)
{
    for (uint8_t addr = 0x20; addr <= 0x27; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, 100 / portTICK_PERIOD_MS);
        i2c_cmd_link_delete(cmd);

        if (err == ESP_OK) {
            pca9535_addr = addr;
            ESP_LOGI(TAG, "Detected PCA9535 at 0x%02x", pca9535_addr);
            return true;
        }
    }

    ESP_LOGE(TAG, "No PCA9535 ACK in 0x20-0x27. Check wiring/power/SDA/SCL/pull-up.");
    return false;
}

esp_err_t pca9535_pre_reset_safe_config(void)
{
    if (!pca9535_detect_addr()) {
        pca9535_ready = false;
        return ESP_FAIL;
    }

    pca9535_ready = true;

    uint8_t write_buf[3] = {REG_CONFIG_PORT0, 0xFF, 0xFF};
    esp_err_t err = i2c_master_write_to_device(I2C_PORT_NUM, pca9535_addr, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Safe config write failed at 0x%02x! err: %s (0x%x)", pca9535_addr, esp_err_to_name(err), err);
        return err;
    }

    // Read input registers once to clear pending interrupt status in PCA9535.
    uint8_t reg = REG_INPUT_PORT0;
    uint8_t input_snapshot[2] = {0};
    err = i2c_master_write_read_device(I2C_PORT_NUM, pca9535_addr, &reg, 1, input_snapshot, sizeof(input_snapshot), 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Safe config input read failed at 0x%02x! err: %s (0x%x)", pca9535_addr, esp_err_to_name(err), err);
        return err;
    }

    config_reg_cache = 0xFFFF;
    ESP_LOGI(TAG, "Safe config applied before reset: REG_CONFIG=0xFFFF, INPUT=0x%02x%02x", input_snapshot[1], input_snapshot[0]);
    return ESP_OK;
}

/*
    * @name   pca9535_init
    * @brief  PCA9535 Initialize
    * @param  None
    * @retval None      
*/
static void pca9535_init(void)
{
    pca9535_ready = pca9535_detect_addr();
    if (!pca9535_ready) {
        return;
    }

    // config IO0_0-IO0_6 and IO1_0-IO1_7 as output (motors and leds), inputs for keys
    // Port 0: 0-3 inputs (KEYS), 4-6 outputs (LED/STBY) -> config = 0x000F
    // Port 1: all outputs (Motors) -> config = 0x0000
    config_reg_cache = 0x000F;
    pca9535_write_reg(REG_CONFIG_PORT0, config_reg_cache);
    
    // Set all outputs low initially
    out_reg_cache = 0x0000;
    pca9535_write_reg(REG_OUTPUT_PORT0, out_reg_cache);
}

/*
    * @name   pca9535_set_pin
    * @brief  Set PCA9535 Pin Level
    * @param  pin: PCA_Pin_t, level: 1/0
    * @retval None      
*/
static void pca9535_set_pin(PCA_Pin_t pin, uint8_t level)
{
    if (!pca9535_ready) {
        return;
    }

    if (level) {
        out_reg_cache |= (1 << pin);
    } else {
        out_reg_cache &= ~(1 << pin);
    }
    pca9535_write_reg(REG_OUTPUT_PORT0, out_reg_cache);
}

/*
    * @name   pca9535_get_pin
    * @brief  Get PCA9535 Pin Level
    * @param  pin: PCA_Pin_t
    * @retval uint8_t      
*/
static uint8_t pca9535_get_pin(PCA_Pin_t pin)
{
    if (!pca9535_ready) {
        return 0;
    }

    uint16_t input_states = pca9535_read_reg(REG_INPUT_PORT0);
    return (input_states >> pin) & 0x01;
}

bool pca9535_is_ready(void)
{
        return pca9535_ready;
}

/********************************************************
  End Of File
********************************************************/