#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "common_interface.h"
#include "gpio_in_out.h"
#include "mqtt_task.h"

#define BLINK_TIME_SHORT 500*1000
#define BLINK_TIME_LED_WIFI 5*1000*1000
#define BLINK_TIME_LED_ETH 3*1000*1000
#define USER_RESET_TIME 3000*1000
#define KEY_SCAN_TIME 20*1000
#define NUM_GPIO 5
uint8_t button_state = 0;
uint8_t led_state = 0;
uint64_t busy_led_time = 0;
uint64_t perivos_time_button = 0;
uint64_t prev_key_scan= 0;

static const char *TAG = "GPIO";
const gpio_num_t input_pins[NUM_GPIO] = {PIN_IN_1, PIN_IN_2, PIN_IN_3, PIN_IN_4, PIN_IN_5};
int led_pub_state[5] = { 1, 1, 1, 1, 1 };
// static int last_state[5] = { -1, -1, -1, -1, -1 };
void check_gpio_and_publish(void) {
    static int last_state[5] = {0};  // Lưu trạng thái trước đó
    int current_state[5];

    // Đọc tất cả trạng thái GPIO
    for (int i = 0; i < NUM_GPIO; i++) {
        current_state[i] = gpio_get_level(input_pins[i]);
    }

    // Kiểm tra nếu có thay đổi
    bool changed = false;
    for (int i = 0; i < 5; i++) {
        if (current_state[i] != last_state[i]) {
            led_pub_state[i] = current_state[i];
            changed = true;
        }
    }

    if (changed) {
        gateway_data.pub_led = true;
        memcpy(last_state, current_state, sizeof(last_state));  // Cập nhật trạng thái mới
    } 
}

void active_alarm(){
    uint8_t in1  = gpio_get_level(PIN_IN_1);
    uint8_t in2  = gpio_get_level(PIN_IN_2);
    uint8_t in3  = gpio_get_level(PIN_IN_3);
    uint8_t in4  = gpio_get_level(PIN_IN_4);
    uint8_t in5  = gpio_get_level(PIN_IN_5);
    if(in1 == 0 || in2 == 0 || in3 == 0 || in4 == 0 || in5 == 0){
        gpio_set_level(PIN_OUT, 1);
    } else{
        gpio_set_level(PIN_OUT, 0);
    }
}

static void pins_init() {
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = GPIO_OUT_PIN,
    };
    gpio_config(&io_conf);

   
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_IN_PIN;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
}
static void button_handler(){
    if (gpio_get_level(PIN_TEST) == 0)
    {
        switch (button_state)
        {
        case 0:
            button_state = 1;
            perivos_time_button = esp_timer_get_time();
            break;
        case 1:
            if (esp_timer_get_time() - perivos_time_button > USER_RESET_TIME)
            {
                ESP_LOGI(TAG, "Resetting WiFi configuration...");            
                ESP_ERROR_CHECK(esp_wifi_stop());  // Dừng Wi-Fi hiện tại
                ESP_ERROR_CHECK(nvs_flash_init());  // Khởi tạo NVS
                ESP_ERROR_CHECK(nvs_flash_erase());  // Xóa cấu hình Wi-Fi
                ESP_LOGI(TAG, "WiFi configuration reset done.");
                esp_restart();  // Khởi động lại thiết bị
            }
            break;
        default:
            button_state = 0;
            break;
        }
    }
}

static void handle_led_blink(int long_time){
    switch (led_state) {
    case 0:
        busy_led_time = esp_timer_get_time();
        led_state = 1;
        gpio_set_level(LED_OUT, 0);
        break;
    case 1:
        if ((esp_timer_get_time() - busy_led_time > BLINK_TIME_SHORT)) {
            busy_led_time = esp_timer_get_time();
            gpio_set_level(LED_OUT, 1);
            led_state = 2;
        }
        break;
    case 2:
        if ((esp_timer_get_time() - busy_led_time > long_time)) {
            led_state = 0;
        }
        break;
    default:
        led_state = 0;
        break;
    }
}


static void button_task(void *arg) {
    while (1) {
        // int button_state = gpio_get_level(PIN_TEST);
        // gpio_set_level(LED_OUT, !button_state); // LED sáng khi nút nhấn được nhấn
        if(gateway_data.status_ap){
            gpio_set_level(LED_OUT, 0);
        } else if(gateway_data.status_eth && gateway_data.mqtt_connect){
            handle_led_blink(BLINK_TIME_SHORT);
        } else if(gateway_data.status_wifi && gateway_data.mqtt_connect){
            handle_led_blink(BLINK_TIME_LED_WIFI);
        } else{
            gpio_set_level(LED_OUT, 1);
        }
        if (esp_timer_get_time() - prev_key_scan > KEY_SCAN_TIME) {
            prev_key_scan = esp_timer_get_time();
            button_handler();
        }
        check_gpio_and_publish();
        active_alarm();

        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}

void gpio_start(){
    pins_init();
    xTaskCreatePinnedToCore(button_task, "button_task", 2048, NULL, 5, NULL,1);
}