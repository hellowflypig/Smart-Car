/* Includes ------------------------------------------------------------------*/
#include "oled.h"
#include "driver/i2c.h"
#include "oledfont.h"
#include <math.h>

/* Private define-------------------------------------------------------------*/
#define OLED_CMD  0x00
#define OLED_DATA 0x40

/* Private variables----------------------------------------------------------*/
static const char *TAG = "OLED";
static uint8_t OLED_GRAM[128][8]; // 128*64 / 8
static uint8_t s_oled_i2c_addr = OLED_I2C_ADDR;

/* Private function prototypes------------------------------------------------*/
static void oled_init(void);
static void oled_clear(void);
static void oled_show_string(uint8_t x, uint8_t y, const char *str, uint8_t size);
static void oled_show_num(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size);
static void oled_refresh(void);
static void oled_draw_point(uint8_t x, uint8_t y, uint8_t mode);
static void oled_show_char(uint8_t x, uint8_t y, uint8_t chr, uint8_t size, uint8_t mode);

/* Public variables-----------------------------------------------------------*/
OLED_t OLED = {
    oled_init,
    oled_clear,
    oled_show_string,
    oled_show_num,
    oled_refresh
};

/* Private function definitions-----------------------------------------------*/

static void OLED_Write_Byte(uint8_t data, uint8_t cmd)
{
    uint8_t write_buf[2] = {cmd, data};
    esp_err_t err = i2c_master_write_to_device(I2C_PORT_NUM, s_oled_i2c_addr, write_buf, 2, 1000 / portTICK_PERIOD_MS);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "I2C Write Failed! addr:0x%02x data:0x%02x cmd:0x%02x err:0x%x", s_oled_i2c_addr, data, cmd, err);
    }
}

bool oled_set_i2c_addr(uint8_t addr)
{
    if (addr != 0x3C && addr != 0x3D) {
        ESP_LOGE(TAG, "Unsupported OLED I2C addr: 0x%02x", addr);
        return false;
    }

    s_oled_i2c_addr = addr;
    ESP_LOGI(TAG, "OLED I2C addr set to 0x%02x", s_oled_i2c_addr);
    return true;
}

uint8_t oled_get_i2c_addr(void)
{
    return s_oled_i2c_addr;
}

/*
    * @name   oled_refresh
    * @brief  Update GRAM to OLED screen
    * @param  None
    * @retval None      
*/
static void oled_refresh(void)
{
    uint8_t i,n;
    for(i=0; i<8; i++) {
        OLED_Write_Byte(0xb0+i, OLED_CMD); // Set page address
        OLED_Write_Byte(0x00, OLED_CMD);   // Set lower column address
        OLED_Write_Byte(0x10, OLED_CMD);   // Set higher column address
        for(n=0; n<128; n++) {
            OLED_Write_Byte(OLED_GRAM[n][i], OLED_DATA);
        }
    }
}

/*
    * @name   oled_clear
    * @brief  Clear OLED screen
    * @param  None
    * @retval None      
*/
static void oled_clear(void)
{
    uint8_t i,n;
    for(i=0; i<8; i++) {
        for(n=0; n<128; n++) {
            OLED_GRAM[n][i] = 0;
        }
    }
    oled_refresh();
}

/*
    * @name   oled_init
    * @brief  Initialize SSD1306 OLED
    * @param  None
    * @retval None      
*/
static void oled_init(void)
{
    ESP_LOGI(TAG, "Initializing OLED...");
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    OLED_Write_Byte(0xAE, OLED_CMD); // display off
    OLED_Write_Byte(0x20, OLED_CMD); // Set Memory Addressing Mode
    OLED_Write_Byte(0x10, OLED_CMD); // 00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
    OLED_Write_Byte(0xb0, OLED_CMD); // Set Page Start Address for Page Addressing Mode,0-7
    OLED_Write_Byte(0xc8, OLED_CMD); // Set COM Output Scan Direction
    OLED_Write_Byte(0x00, OLED_CMD); // set low column address
    OLED_Write_Byte(0x10, OLED_CMD); // set high column address
    OLED_Write_Byte(0x40, OLED_CMD); // set start line address
    OLED_Write_Byte(0x81, OLED_CMD); // set contrast control register
    OLED_Write_Byte(0xff, OLED_CMD);
    OLED_Write_Byte(0xa1, OLED_CMD); // set segment re-map 0 to 127
    OLED_Write_Byte(0xa6, OLED_CMD); // set normal display
    OLED_Write_Byte(0xa8, OLED_CMD); // set multiplex ratio(1 to 64)
    OLED_Write_Byte(0x3F, OLED_CMD); // 
    OLED_Write_Byte(0xa4, OLED_CMD); // 0xa4,Output follows RAM content;0xa5,Output ignores RAM content
    OLED_Write_Byte(0xd3, OLED_CMD); // set display offset
    OLED_Write_Byte(0x00, OLED_CMD); // no offset
    OLED_Write_Byte(0xd5, OLED_CMD); // set display clock divide ratio/oscillator frequency
    OLED_Write_Byte(0xf0, OLED_CMD); // set divide ratio
    OLED_Write_Byte(0xd9, OLED_CMD); // set pre-charge period
    OLED_Write_Byte(0x22, OLED_CMD); // 
    OLED_Write_Byte(0xda, OLED_CMD); // set com pins hardware configuration
    OLED_Write_Byte(0x12, OLED_CMD);
    OLED_Write_Byte(0xdb, OLED_CMD); // set vcomh
    OLED_Write_Byte(0x20, OLED_CMD); // 0x20,0.77xVcc
    OLED_Write_Byte(0x8d, OLED_CMD); // set DC-DC enable
    OLED_Write_Byte(0x14, OLED_CMD); // 
    OLED_Write_Byte(0xaf, OLED_CMD); // turn on oled panel
    
    oled_clear();
}

/*
    * @name   oled_draw_point
    * @brief  Draw a point in GRAM
    * @param  x: 0-127
    * @param  y: 0-63
    * @param  mode: 1 (set), 0 (clear)
*/
static void oled_draw_point(uint8_t x, uint8_t y, uint8_t mode)
{
    if(x > 127 || y > 63) return;
    if(mode) OLED_GRAM[x][y/8] |= 1 << (y%8);
    else     OLED_GRAM[x][y/8] &= ~(1 << (y%8));
}

/*
    * @name   oled_show_char
    * @brief  Show a single character
    * @param  x: 0-127
    * @param  y: 0-63
    * @param  chr: character to show
    * @param  size: 8 or 16
    * @param  mode: 1 (normal), 0 (reverse)
*/
static void oled_show_char(uint8_t x, uint8_t y, uint8_t chr, uint8_t size, uint8_t mode)
{
    uint8_t temp, t, t1;
    uint8_t y0 = y;
    uint8_t csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);
    chr = chr - ' '; // Offset for font array
    
    for(t = 0; t < csize; t++) {
        if(size == 8) temp = asc2_0806[chr][t];
        else if(size == 16) temp = asc2_1608[chr][t];
        else return;
        
        for(t1 = 0; t1 < 8; t1++) {
            if(temp & 0x80) oled_draw_point(x, y, mode);
            else            oled_draw_point(x, y, !mode);
            temp <<= 1;
            y++;
            if((y - y0) == size) {
                y = y0;
                x++;
                break;
            }
        }
    }
}

/*
    * @name   oled_show_string
    * @brief  Show string on OLED
*/
static void oled_show_string(uint8_t x, uint8_t y, const char *str, uint8_t size)
{
    while((*str >= ' ') && (*str <= '~')) {
        oled_show_char(x, y, *str, size, 1);
        if(size == 8) x += 6;
        else x += 8;
        str++;
    }
}

/*
    * @name   oled_pow
    * @brief  m^n
*/
static uint32_t oled_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while(n--) result *= m;
    return result;
}

/*
    * @name   oled_show_num
    * @brief  Show number on OLED
*/
static void oled_show_num(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t size)
{
    uint8_t t, temp;
    uint8_t enshow = 0;
    
    // Handle negative
    if(num < 0) {
        oled_show_char(x, y, '-', size, 1);
        num = -num;
        if(size == 8) x += 6;
        else x += 8;
    }
    
    for(t = 0; t < len; t++) {
        temp = (num / oled_pow(10, len - t - 1)) % 10;
        if(enshow == 0 && t < (len - 1)) {
            if(temp == 0) {
                oled_show_char(x + (size / 2) * t, y, ' ', size, 1);
                continue;
            } else {
                enshow = 1;
            }
        }
        oled_show_char(x + (size / 2) * t, y, temp + '0', size, 1);
    }
}

/********************************************************
  End Of File
********************************************************/