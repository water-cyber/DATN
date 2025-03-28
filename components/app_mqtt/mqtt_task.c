#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "common_interface.h"
#include "cJSON.h"
#include "rs485.h"
#include "ds1307.h"
#define BROKER_ADDRESS "mqtt://dev-emqx.sful.com.vn"
#define MQTT_USER "FCVD-01"
#define MQTT_PASSWORD "xtijGhP9bKLBi0S"
#define TOPIC_LED "gateway/led"
#define TOPIC_RS485 "gateway/rs485"
#define TOPIC_DOWN "gateway/down"
static const char *TAG = "MQTT_APP";
esp_mqtt_event_handle_t event ;
esp_mqtt_client_handle_t client ;
// Hàm xử lý sự kiện MQTT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    event = event_data;
    client = event->client;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Đã kết nối MQTT");
            esp_mqtt_client_subscribe(client, TOPIC_DOWN, 0);
            gateway_data.mqtt_connect = true;
            //esp_mqtt_client_publish(client, "esp32/topic", "Hello from ESP32", 0, 1, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT bị ngắt kết nối");
            gateway_data.mqtt_connect = false;
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "Đã đăng ký chủ đề thành công");
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "Đã huỷ đăng ký chủ đề");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "Đã gửi dữ liệu MQTT");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Nhận dữ liệu: %.*s", event->data_len, event->data);
            if (event->data_len == 1) {  // Kiểm tra độ dài hợp lệ
                if (event->data[0] == '1') {
                    gateway_data.relay = true;
                } else if (event->data[0] == '0') {
                    gateway_data.relay = false;
                }
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT lỗi");
            gateway_data.mqtt_connect = false;
            break;
        default:
            ESP_LOGI(TAG, "MQTT sự kiện khác: %d", event->event_id);
            gateway_data.mqtt_connect = false;
            break;
    }
}



void publish_mqtt_inputs(int inputs[5]) {
    rtc_time_t now;
    if (ds1307_get_time(&now) == ESP_OK) {
        ESP_LOGI(TAG, "Giờ hiện tại: %02d:%02d:%02d - %02d/%02d/20%02d",
                 now.hour, now.minute, now.second, now.date, now.month, now.year);
    }

    // Tạo JSON
    cJSON *root = cJSON_CreateObject();
    cJSON *input_array = cJSON_CreateIntArray(inputs, 5);
    cJSON_AddItemToObject(root, "inputs", input_array);

    // Thêm thời gian vào JSON
    cJSON_AddNumberToObject(root, "hour", now.hour);
    cJSON_AddNumberToObject(root, "minute", now.minute);
    cJSON_AddNumberToObject(root, "second", now.second);
    cJSON_AddNumberToObject(root, "day", now.date);
    cJSON_AddNumberToObject(root, "month", now.month);
    cJSON_AddNumberToObject(root, "year", 2000 + now.year); // Chuyển về năm đầy đủ

    char *json_str = cJSON_PrintUnformatted(root);
    ESP_LOGI(TAG, "Sending JSON: %s", json_str);

    // Publish lên MQTT
    esp_mqtt_client_publish(client, TOPIC_LED, json_str, 0, 1, 0);

    // Giải phóng bộ nhớ
    cJSON_Delete(root);
    free(json_str);
}

void mqtt_task(void *pvParameters) {
    while (1) {
        if(gateway_data.mqtt_connect){
            char *received_data = rs485_receive();
            if (received_data)
            {
                esp_mqtt_client_publish(client, TOPIC_RS485, received_data, 0, 0, 0);
                ESP_LOGI(TAG, "pub RS485 thành công");
            }

            if (gateway_data.pub_led) {
                publish_mqtt_inputs(led_pub_state);
                ESP_LOGI(TAG, "pub LED thành công");
                gateway_data.pub_led = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));  // Kiểm tra mỗi 50ms
    }
}
// Hàm khởi tạo MQTT
void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_ADDRESS,  // Địa chỉ broker MQTT
        .credentials.username = MQTT_USER,  // Tên người dùng MQTT
        .credentials.authentication.password = MQTT_PASSWORD,  // Mật khẩu MQTT
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);

    xTaskCreatePinnedToCore(mqtt_task, "mqtt_task", 4096, NULL, 5, NULL,0);
}
