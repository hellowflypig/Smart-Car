/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "wifi_app.h"
#include "camera.h"
#include "motor.h"
#include "servo.h"

#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

/* Private define-------------------------------------------------------------*/
#define WIFI_SSID      "SMART_CAR_AP"
#define WIFI_PASS      "12345678"
#define CMD_PORT       8080
#define VIDEO_PORT     8081

/* Private variables----------------------------------------------------------*/
static const char *TAG = "WIFI_APP";
static int cmd_sock = -1;
static int video_sock = -1;
static struct sockaddr_in remote_addr; // Store the address of the connected remote

/* Private function prototypes------------------------------------------------*/
static void wifi_init_softap(void);
static void udp_server_task(void *pvParameters);
static void video_stream_task(void *pvParameters);
static void wifi_app_init(void);

/* Public variables-----------------------------------------------------------*/
WiFi_App_t WiFiApp = {
    wifi_app_init
};

/* Private function definitions-----------------------------------------------*/

/*
    * @name   wifi_init_softap
    * @brief  Initialize WiFi as Access Point
*/
static void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = 1,
            .password = WIFI_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP initialized. SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
}

/*
    * @name   udp_server_task
    * @brief  Task to receive control commands
*/
static void udp_server_task(void *pvParameters)
{
    char rx_buffer[128];

    while (1) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(CMD_PORT);

        cmd_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (cmd_sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        bind(cmd_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

        while (1) {
            struct sockaddr_storage source_addr;
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(cmd_sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            } else {
                rx_buffer[len] = 0; // Null-terminate
                
                // Save remote address for video streaming
                if (source_addr.ss_family == AF_INET) {
                    remote_addr = *(struct sockaddr_in *)&source_addr;
                    remote_addr.sin_port = htons(VIDEO_PORT); // Send video to remote's VIDEO_PORT
                }

                // Process command (Simple parser)
                // Assuming format: "M L_SPEED R_SPEED" for motor
                // Assuming format: "S ANGLE" for servo
                if (rx_buffer[0] == 'M') {
                    int l_spd=0, r_spd=0;
                    sscanf(rx_buffer, "M %d %d", &l_spd, &r_spd);
                    // Use open-loop speed for direct control
                    CarMotor.set_speed(MOTOR_LF, l_spd);
                    CarMotor.set_speed(MOTOR_LR, l_spd);
                    CarMotor.set_speed(MOTOR_RF, r_spd);
                    CarMotor.set_speed(MOTOR_RR, r_spd);
                } else if (rx_buffer[0] == 'S') {
                    float angle = 0;
                    sscanf(rx_buffer, "S %f", &angle);
                    CameraServo.set_angle(angle);
                }
            }
        }
        if (cmd_sock != -1) {
            shutdown(cmd_sock, 0);
            close(cmd_sock);
        }
    }
    vTaskDelete(NULL);
}

/*
    * @name   video_stream_task
    * @brief  Task to grab frames and send via UDP to remote
*/
static void video_stream_task(void *pvParameters)
{
    video_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (video_sock < 0) {
        ESP_LOGE(TAG, "Unable to create video socket");
        vTaskDelete(NULL);
    }

    while (1) {
        // Only send if we have a valid remote client
        if (remote_addr.sin_family == AF_INET) {
            camera_fb_t *fb = CarCamera.get_frame();
            if (fb) {
                // Send frame. NOTE: For UDP, if JPEG is larger than MTU (~1472 bytes),
                // it needs to be fragmented. For simplicity in this demo, 
                // send in chunks.
                size_t chunk_size = 1024;
                for (size_t i = 0; i < fb->len; i += chunk_size) {
                    size_t send_len = (fb->len - i < chunk_size) ? (fb->len - i) : chunk_size;
                    sendto(video_sock, fb->buf + i, send_len, 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
                    // Small delay to prevent UDP packet loss
                    vTaskDelay(1 / portTICK_PERIOD_MS); 
                }
                CarCamera.return_frame(fb);
            }
        } else {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}

/*
    * @name   wifi_app_init
    * @brief  Initialize WiFi and start tasks
*/
static void wifi_app_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi AP & UDP Servers...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    wifi_init_softap();

    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
    xTaskCreatePinnedToCore(video_stream_task, "video_stream", 8192, NULL, 5, NULL, 1);
}

/********************************************************
  End Of File
********************************************************/