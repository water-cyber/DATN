#include "ds1307.h"

#define I2C_MASTER_SCL_IO          22       // Chân SCL
#define I2C_MASTER_SDA_IO          21       // Chân SDA
#define I2C_MASTER_NUM             I2C_NUM_0 // Kênh I2C
#define I2C_MASTER_FREQ_HZ         100000   // Tốc độ I2C
#define I2C_MASTER_TX_BUF_DISABLE  0
#define I2C_MASTER_RX_BUF_DISABLE  0

#define DS1307_ADDR                0x68  // Địa chỉ I2C của DS1307

static const char *TAG = "DS1307";

// Hàm chuyển đổi BCD <-> Decimal
uint8_t bcd_to_dec(uint8_t val) {
    return ((val / 16 * 10) + (val % 16));
}

uint8_t dec_to_bcd(uint8_t val) {
    return ((val / 10 * 16) + (val % 10));
}

// Hàm khởi tạo I2C
esp_err_t i2c_master_init(void) {
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(i2c_master_port, &conf);
    if (err != ESP_OK) return err;
    
    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

// Hàm ghi thời gian vào DS1307
esp_err_t ds1307_set_time(rtc_time_t time) {
    uint8_t data[8];
    data[0] = 0x00; // Địa chỉ bắt đầu ghi
    data[1] = dec_to_bcd(time.second);
    data[2] = dec_to_bcd(time.minute);
    data[3] = dec_to_bcd(time.hour);
    data[4] = dec_to_bcd(time.day);
    data[5] = dec_to_bcd(time.date);
    data[6] = dec_to_bcd(time.month);
    data[7] = dec_to_bcd(time.year);

    return i2c_master_write_to_device(I2C_MASTER_NUM, DS1307_ADDR, data, sizeof(data), 1000 / portTICK_PERIOD_MS);
}

// Hàm đọc thời gian từ DS1307
esp_err_t ds1307_get_time(rtc_time_t *time) {
    uint8_t reg_addr = 0x00;
    uint8_t data[7];

    // Gửi địa chỉ bộ nhớ cần đọc
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, DS1307_ADDR, &reg_addr, 1, data, 7, 1000 / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi đọc thời gian từ DS1307");
        return err;
    }

    // Chuyển đổi từ BCD sang số thập phân
    time->second = bcd_to_dec(data[0] & 0x7F);
    time->minute = bcd_to_dec(data[1]);
    time->hour = bcd_to_dec(data[2]);
    time->day = bcd_to_dec(data[3]);
    time->date = bcd_to_dec(data[4]);
    time->month = bcd_to_dec(data[5]);
    time->year = bcd_to_dec(data[6]);

    return ESP_OK;
}

