#ifndef DS1307_H
#define DS1307_H

#include <stdio.h>
#include "driver/i2c.h"
#include "esp_log.h"

// Cấu trúc lưu thời gian
typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t date;
    uint8_t month;
    uint8_t year;
} rtc_time_t;

esp_err_t i2c_master_init(void);
esp_err_t ds1307_set_time(rtc_time_t time);
esp_err_t ds1307_get_time(rtc_time_t *time);

#endif /* DS1307_H */