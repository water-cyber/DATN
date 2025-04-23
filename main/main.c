#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "wifi_sta.h"
#include "ethernet_w5500.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "gpio_in_out.h"
#include "common_interface.h"
#include "ds1307.h"

#include "mqtt_task.h"
#include "rs485.h"

#define TAG "MAIN"
static bool wifi_stopped = false;
gateway_data_t gateway_data;

bool is_wifi_configured() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return false;  // Không tìm thấy dữ liệu
    }

    size_t len;
    err = nvs_get_str(nvs_handle, "ssid", NULL, &len);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return false;
    }

    nvs_close(nvs_handle);
    return true;  // SSID đã được lưu
}

void check_eth_wifi_status() {
    if (!eth_netif) {
        ESP_LOGW(TAG, "Ethernet interface not found!");
        return;
    }

    if (esp_netif_is_netif_up(eth_netif)) {
        if (!wifi_stopped) {
            ESP_LOGI(TAG, "Ethernet UP, stopping Wi-Fi.");
            gateway_data.status_eth = true;
            gateway_data.status_wifi = false;
            esp_wifi_stop();
            wifi_stopped = true;
        }
    } else {
        gateway_data.status_wifi = true;
        if (wifi_stopped) {
            ESP_LOGI(TAG, "Ethernet DOWN, starting Wi-Fi.");
            esp_wifi_start();
            
            gateway_data.status_eth = false;
            wifi_stopped = false;
        }
    }
}

void network_switch_task(void *arg) {
    while (1)
    {
        check_eth_wifi_status();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    } 
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(i2c_master_init());
    gpio_start();
    rs485_init();
    lora_init();
    
    if (!is_wifi_configured()){
        gateway_data.status_ap = true;
        wifi_init_softap();
        start_web_server();
        wait_for_wifi_credentials();
    }
    gateway_data.status_ap = false;
    ethernet_w5500_init();
    wifi_init_sta();
    xTaskCreatePinnedToCore(network_switch_task, "network_switch_task", 4096, NULL, 5, NULL,0);
    mqtt_app_start();
    
    // Cài đặt thời gian ban đầu
    rtc_time_t set_time = { .second = 0, .minute = 6, .hour = 13, .day = 4, .date = 3, .month = 4, .year = 25 }; // 15:30:00, Thứ 5, 22/03/2024
    ESP_ERROR_CHECK(ds1307_set_time(set_time));
    ESP_LOGI(TAG, "Đã ghi thời gian vào DS1307");

    // Đọc thời gian liên tục từ DS1307
    // rtc_time_t now;
    // while (1) {
    //     if (ds1307_get_time(&now) == ESP_OK) {
    //         ESP_LOGI(TAG, "Giờ hiện tại: %02d:%02d:%02d - %02d/%02d/20%02d",
    //                  now.hour, now.minute, now.second, now.date, now.month, now.year);
    //     }
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
}
