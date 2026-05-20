/* Includes ------------------------------------------------------------------*/
#include "bsp_i2c.h"
#include "esp_rom_sys.h"

/* Private variables----------------------------------------------------------*/
static const char *TAG = "BSP_I2C";

/* Private function prototypes------------------------------------------------*/
static void bsp_i2c_init(void);
static esp_err_t bsp_i2c_try_init(gpio_num_t sda, gpio_num_t scl, uint32_t clk_hz);
static uint8_t bsp_i2c_scan_bus(void);
static void bsp_i2c_line_sanity_check(gpio_num_t sda, gpio_num_t scl);
static void bsp_i2c_bus_recovery(gpio_num_t sda, gpio_num_t scl);

/* Public variables-----------------------------------------------------------*/
BSP_I2C_t I2C_Master = {
    bsp_i2c_init
};

/* Private function definitions-----------------------------------------------*/

static void bsp_i2c_line_sanity_check(gpio_num_t sda, gpio_num_t scl)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << sda) | (1ULL << scl),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    gpio_set_level(sda, 1);
    gpio_set_level(scl, 1);
    esp_rom_delay_us(20);
    ESP_LOGI(TAG, "Line idle level SDA:%d SCL:%d",
             gpio_get_level(sda), gpio_get_level(scl));

    gpio_set_level(scl, 0);
    esp_rom_delay_us(20);
    ESP_LOGI(TAG, "Drive SCL low -> SDA:%d SCL:%d",
             gpio_get_level(sda), gpio_get_level(scl));

    gpio_set_level(scl, 1);
    gpio_set_level(sda, 0);
    esp_rom_delay_us(20);
    ESP_LOGI(TAG, "Drive SDA low -> SDA:%d SCL:%d",
             gpio_get_level(sda), gpio_get_level(scl));

    gpio_set_level(sda, 1);
    gpio_set_level(scl, 1);
    esp_rom_delay_us(20);
    ESP_LOGI(TAG, "Line release -> SDA:%d SCL:%d",
             gpio_get_level(sda), gpio_get_level(scl));
}

static void bsp_i2c_bus_recovery(gpio_num_t sda, gpio_num_t scl)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << sda) | (1ULL << scl),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    gpio_set_level(sda, 1);
    gpio_set_level(scl, 1);
    esp_rom_delay_us(20);

    // 9 SCL pulses to recover bus if a slave is stuck in transmit state.
    for (int i = 0; i < 9; i++) {
        gpio_set_level(scl, 0);
        esp_rom_delay_us(5);
        gpio_set_level(scl, 1);
        esp_rom_delay_us(5);
    }

    // STOP condition: SDA low -> SCL high -> SDA high.
    gpio_set_level(sda, 0);
    esp_rom_delay_us(5);
    gpio_set_level(scl, 1);
    esp_rom_delay_us(5);
    gpio_set_level(sda, 1);
    esp_rom_delay_us(5);

    ESP_LOGI(TAG, "Bus recovery done, SDA:%d SCL:%d",
             gpio_get_level(sda), gpio_get_level(scl));
}

static esp_err_t bsp_i2c_try_init(gpio_num_t sda, gpio_num_t scl, uint32_t clk_hz)
{
    gpio_reset_pin(sda);
    gpio_reset_pin(scl);

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = clk_hz,
    };

    esp_err_t err = i2c_param_config(I2C_PORT_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed on SDA:%d SCL:%d err:%s (0x%x)",
                 sda, scl, esp_err_to_name(err), err);
        return err;
    }

    err = i2c_driver_install(I2C_PORT_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed on SDA:%d SCL:%d err:%s (0x%x)",
                 sda, scl, esp_err_to_name(err), err);
        return err;
    }

    ESP_LOGI(TAG, "I2C Master Init Successfully on SDA:%d SCL:%d @ %luHz",
             sda, scl, (unsigned long)clk_hz);
    return ESP_OK;
}

static uint8_t bsp_i2c_scan_bus(void)
{
    uint8_t devices_found = 0;
    uint16_t nack_cnt = 0;
    uint16_t timeout_cnt = 0;
    uint16_t other_err_cnt = 0;

    ESP_LOGI(TAG, "Scanning I2C bus...");
    for (int i = 1; i < 127; i++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Found I2C device at 7-bit: 0x%02x (8-bit write: 0x%02x)", i, (i << 1));
            devices_found++;
        } else if (ret == ESP_FAIL) {
            nack_cnt++;
        } else if (ret == ESP_ERR_TIMEOUT) {
            timeout_cnt++;
        } else {
            other_err_cnt++;
        }
    }

    if (devices_found == 0) {
        ESP_LOGE(TAG, "No I2C devices found! nack=%u timeout=%u other=%u",
                 nack_cnt, timeout_cnt, other_err_cnt);
    } else {
        ESP_LOGI(TAG, "I2C Scan complete. Found %u device(s). nack=%u timeout=%u other=%u",
                 devices_found, nack_cnt, timeout_cnt, other_err_cnt);
    }

    return devices_found;
}

/*
    * @name   bsp_i2c_init
    * @brief  Initialize I2C Master bus for PCA9535, OLED, and Camera
    * @param  None
    * @retval None      
*/
static void bsp_i2c_init(void)
{
    bsp_i2c_line_sanity_check(I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    bsp_i2c_bus_recovery(I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);

    esp_err_t err = bsp_i2c_try_init(I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, 100000);
    if (err != ESP_OK) {
        return;
    }

    uint8_t found = bsp_i2c_scan_bus();
    if (found > 0) {
        return;
    }

    ESP_LOGW(TAG, "No device found on SDA:%d SCL:%d, retry with swapped pins for PCB check",
             I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);

    i2c_driver_delete(I2C_PORT_NUM);

    err = bsp_i2c_try_init(I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO, 100000);
    if (err != ESP_OK) {
        return;
    }

    found = bsp_i2c_scan_bus();
    if (found > 0) {
        ESP_LOGW(TAG, "I2C works only with swapped pins. Please check PCB/net labels for SDA/SCL cross.");
    } else {
        ESP_LOGE(TAG, "Both normal and swapped pin mapping failed.");
    }
}

/********************************************************
  End Of File
********************************************************/
