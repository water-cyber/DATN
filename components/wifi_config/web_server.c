#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <ctype.h>
#include "web_server.h"

#define MAX_APs 20 
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64
static const char *TAG = "web_server";

static EventGroupHandle_t ap_event_group;
const int WIFI_CREDENTIALS_READY = BIT0; // Bit báo hiệu SSID/PASS đã sẵn sàng
// HTML hiển thị danh sách WiFi và form nhập mật khẩu
const char *HTML_PAGE = "<!DOCTYPE html>\
<html><head><meta charset='UTF-8'><title>ESP32 WiFi Setup</title></head>\
<body><h2>Chọn WiFi</h2>\
<form action='/connect' method='POST'>\
<label>SSID:</label><select name='ssid'>%s</select><br>\
<label>Password:</label><input type='password' name='password'><br>\
<input type='submit' value='Kết Nối'>\
</form></body></html>";

// Lưu SSID/PASS vào NVS
void save_wifi_config(const char *ssid, const char *password) {
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("wifi_config", NVS_READWRITE, &nvs_handle));
    nvs_set_str(nvs_handle, "ssid", ssid);
    nvs_set_str(nvs_handle, "password", password);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "WiFi config saved!");
}

// Xử lý quét WiFi và trả về danh sách SSID
static esp_err_t scan_wifi_handler(httpd_req_t *req) {
    wifi_scan_config_t scan_config = {    
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,};
    esp_wifi_scan_start(&scan_config, true);

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    wifi_ap_record_t ap_records[ap_count];
    esp_wifi_scan_get_ap_records(&ap_count, ap_records);

    char ssid_list[512] = "";
    for (int i = 0; i < ap_count; i++) {
        char option[128];
        snprintf(option, sizeof(option), "<option value='%s'>%s (%d dBm)</option>", 
                 ap_records[i].ssid, ap_records[i].ssid, ap_records[i].rssi);
        strcat(ssid_list, option);
    }

    char response_html[1024];
    snprintf(response_html, sizeof(response_html), HTML_PAGE, ssid_list);
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Hàm giải mã URL
void url_decode(const char *src, char *dest, size_t dest_size) {
    char *d = dest;
    const char *s = src;
    char hex[3] = {0};

    while (*s && (d - dest) < (dest_size - 1)) {
        if (*s == '%') {
            if (isxdigit((unsigned char) *(s + 1)) && isxdigit((unsigned char) *(s + 2))) {
                hex[0] = *(s + 1);
                hex[1] = *(s + 2);
                *d++ = (char) strtol(hex, NULL, 16);
                s += 3;
            } else {
                *d++ = *s++; // Trường hợp `%` nhưng không hợp lệ
            }
        } else if (*s == '+') {
            *d++ = ' ';
            s++;
        } else {
            *d++ = *s++;
        }
    }
    *d = '\0'; // Kết thúc chuỗi
}

// Xử lý nhận SSID/PASS từ form và lưu vào NVS
static esp_err_t connect_wifi_handler(httpd_req_t *req) {
    char content[100];  // Bộ đệm chứa query string
    int recv_len = httpd_req_recv(req, content, sizeof(content) - 1);
    if (recv_len <= 0) return ESP_FAIL;
    content[recv_len] = '\0';
    ESP_LOGI(TAG, "Nhận dữ liệu từ Web: %s", content);

    char ssid_enc[MAX_SSID_LEN] = {0}, password_enc[MAX_PASS_LEN] = {0};
    char ssid[MAX_SSID_LEN] = {0}, password[MAX_PASS_LEN] = {0};

    // Lấy giá trị SSID từ query string
    if (httpd_query_key_value(content, "ssid", ssid_enc, sizeof(ssid_enc)) == ESP_OK) {
        url_decode(ssid_enc, ssid, sizeof(ssid));  // Giải mã URL
        ESP_LOGI(TAG, "SSID: %s", ssid);
    } else {
        ESP_LOGE(TAG, "Không tìm thấy SSID!");
        httpd_resp_send(req, "Missing SSID", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    // Lấy giá trị Password từ query string
    if (httpd_query_key_value(content, "password", password_enc, sizeof(password_enc)) == ESP_OK) {
        url_decode(password_enc, password, sizeof(password));  // Giải mã URL
        ESP_LOGI(TAG, "Password: %s", password);
    } else {
        ESP_LOGE(TAG, "Không tìm thấy Password!");
        httpd_resp_send(req, "Missing Password", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    // Lưu SSID/PASS vào NVS (giả định có hàm save_wifi_config)
    save_wifi_config(ssid, password);

    httpd_resp_send(req, "Connection successful, ESP will reboot!", HTTPD_RESP_USE_STRLEN);
    xEventGroupSetBits(ap_event_group, WIFI_CREDENTIALS_READY);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    esp_restart();
    return ESP_OK;
}

// Khởi động Web Server
void start_web_server() {
    ap_event_group = xEventGroupCreate(); // Tạo Event Group
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192; // Tăng stack size
    httpd_handle_t server = NULL;
    httpd_start(&server, &config);

    httpd_uri_t scan_wifi = {.uri = "/", .method = HTTP_GET, .handler = scan_wifi_handler};
    httpd_uri_t connect_wifi = {.uri = "/connect", .method = HTTP_POST, .handler = connect_wifi_handler};

    httpd_register_uri_handler(server, &scan_wifi);
    httpd_register_uri_handler(server, &connect_wifi);
    ESP_LOGI(TAG, "Web server started");
}
void wait_for_wifi_credentials() {
    ESP_LOGI(TAG, "Waiting SSID/PASS from Web...");
    xEventGroupWaitBits(ap_event_group, WIFI_CREDENTIALS_READY, pdTRUE, pdFALSE, portMAX_DELAY);
}