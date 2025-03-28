#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_log.h"

#include "esp_event.h"
#include "wifi_manager.h"

#define WIFI_SSID "ESP32_Setup"
#define WIFI_PASS "12345678"
#define MAX_STA_CONN 4

// #define DNS_PORT 53

static const char *TAG = "wifi_manager";
// TaskHandle_t dns_task_handle = NULL;

// void dns_server_task(void *pvParameters) {
//     dns_task_handle = xTaskGetCurrentTaskHandle(); // Lưu ID Task DNS
//     struct sockaddr_in server_addr, client_addr;
//     socklen_t addr_len = sizeof(client_addr);
//     char buffer[512];

//     int sock = socket(AF_INET, SOCK_DGRAM, 0);
//     if (sock < 0) {
//         ESP_LOGE(TAG, "Failed to create socket");
//         vTaskDelete(NULL);
//         return;
//     }

//     server_addr.sin_family = AF_INET;
//     server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
//     server_addr.sin_port = htons(DNS_PORT);

//     if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
//         ESP_LOGE(TAG, "Socket bind failed");
//         close(sock);
//         vTaskDelete(NULL);
//         return;
//     }

//     ESP_LOGI(TAG, "DNS server started on port %d", DNS_PORT);

//     while (1) {
//         int len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &addr_len);
//         if (len > 0) {
//             ESP_LOGI(TAG, "DNS request received");

//             // Trả về IP của ESP32 (192.168.4.1)
//             struct sockaddr_in *addr = (struct sockaddr_in *)&client_addr;
//             addr->sin_addr.s_addr = inet_addr("192.168.4.1");

//             sendto(sock, buffer, len, 0, (struct sockaddr *)&client_addr, addr_len);
//         }
//     }

//     close(sock);
//     vTaskDelete(NULL);
// }

// void start_captive_portal() {
//     xTaskCreatePinnedToCore(dns_server_task, "dns_server_task", 8192, NULL, 5, NULL,0);
// }


void wifi_init_softap(void) {
    ESP_LOGI(TAG, "Initializing WiFi in SoftAP mode...");
    // Tạo network interface cho SoftAP
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP started with SSID: %s", WIFI_SSID);
}
