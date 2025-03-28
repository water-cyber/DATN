#include <stdio.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "rs485.h"
#include <string.h>
#define TAG "RS485"

// Cấu hình UART1 (RS485)
#define UART_NUM UART_NUM_1
#define TXD_PIN  (GPIO_NUM_25) // Chân TX
#define RXD_PIN  (GPIO_NUM_26) // Chân RX
#define RTS_PIN  (GPIO_NUM_27) // Chân DE (Driver Enable)

// Kích thước buffer
#define BUF_SIZE (256)

void rs485_init()
{
    uart_config_t uart_config = {
        .baud_rate = 115200,  // Tốc độ baud
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, // Có thể dùng EVEN nếu cần
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Cấu hình UART1
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, RTS_PIN, UART_PIN_NO_CHANGE);

    // Cấu hình RS485 Half-Duplex mode
    uart_set_mode(UART_NUM, UART_MODE_RS485_HALF_DUPLEX);
    ESP_LOGI(TAG, "RS485 Initialized");
}

void rs485_send(const char *data)
{
    int len = strlen(data);
    uart_write_bytes(UART_NUM, data, len);
    ESP_LOGI(TAG, "Sent: %s", data);
}

char* rs485_receive()
{
    static char data[BUF_SIZE];  // Biến static để giữ dữ liệu sau khi hàm kết thúc
    int len = uart_read_bytes(UART_NUM, (uint8_t*)data, BUF_SIZE - 1, 100 / portTICK_PERIOD_MS);

    if (len > 0)
    {
        data[len] = '\0'; // Kết thúc chuỗi
        ESP_LOGI(TAG, "Received: %s", data);
        return data;
    }

    return NULL; // Không có dữ liệu
}

