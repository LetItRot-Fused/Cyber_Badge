/**
 * @file CyberBadgeRTC.hpp
 * @author LetItRot
 * @brief 赛博吧唧RTC （使用板载的RTC）
 * @version 1.0.0
 * @date 2026-05-26
 * @copyright Copyright (c) 2026 LetItRot
 */

#pragma once

#include <time.h>
#include <sys/time.h>
#include "esp_log.h"

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
} RTC_TimeTypeDef;

typedef struct {
    uint8_t date;
    uint8_t weekDay;
    uint8_t month;
    uint16_t year;
} RTC_DateTypeDef;

class CyberBadgeRTC {
private:
    const char* TAG = "ESP32_RTC";

public:
    CyberBadgeRTC()  = default;
    ~CyberBadgeRTC() = default;

    void init() {
        setenv("TZ", "CST-8", 1);
        tzset();
        ESP_LOGI(TAG, "ESP32 内置 RTC 初始化成功");
    }

    void getTime(RTC_TimeTypeDef* rtc_time) {
        if (!rtc_time) return;
        time_t now = ::time(NULL);
        struct tm tm_info;
        localtime_r(&now, &tm_info);

        rtc_time->hours   = tm_info.tm_hour;
        rtc_time->minutes = tm_info.tm_min;
        rtc_time->seconds = tm_info.tm_sec;
    }

    void setTime(RTC_TimeTypeDef* rtc_time) {
        if (!rtc_time) return;

        time_t now = ::time(NULL);
        struct tm tm_info;
        localtime_r(&now, &tm_info);

        tm_info.tm_hour = rtc_time->hours;
        tm_info.tm_min  = rtc_time->minutes;
        tm_info.tm_sec  = rtc_time->seconds;

        time_t t = mktime(&tm_info);
        struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&tv, NULL);
    }

    void getDate(RTC_DateTypeDef* rtc_date) {
        if (!rtc_date) return;

        time_t now = ::time(NULL);
        struct tm tm_info;
        localtime_r(&now, &tm_info);

        rtc_date->year    = tm_info.tm_year + 1900;
        rtc_date->month   = tm_info.tm_mon + 1;
        rtc_date->date    = tm_info.tm_mday;
        rtc_date->weekDay = tm_info.tm_wday;
    }

    void setDate(RTC_DateTypeDef* rtc_date) {
        if (!rtc_date) return;

        time_t now = ::time(NULL);
        struct tm tm_info;
        localtime_r(&now, &tm_info);

        tm_info.tm_year = rtc_date->year - 1900;
        tm_info.tm_mon  = rtc_date->month - 1;
        tm_info.tm_mday = rtc_date->date;

        time_t t = mktime(&tm_info);
        struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&tv, NULL);
    }
};