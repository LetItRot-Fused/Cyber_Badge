/**
 * @file CyberBadgePower.hpp
 * @author LetItRot
 * @brief 赛博吧唧电池电压检测（无检测开关）
 * @version 1.0.0
 * @date 2026-05-26
 * @copyright Copyright (c) 2026 LetItRot
 */

#ifndef CYBERBADGE_POWER_HPP
#define CYBERBADGE_POWER_HPP

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"

#define ADC1_CHANNEL         ADC_CHANNEL_0
#define ADC_ATTEN            ADC_ATTEN_DB_0
#define ADC_BIT_WIDTH        ADC_BITWIDTH_12

static const char * POWER_TAG = "CyberPower";

class CyberBadgePower {
private:
    adc_oneshot_unit_handle_t _adc1_handle = NULL;

public:
    CyberBadgePower() {}
    ~CyberBadgePower() {}

    bool init(void)
    {
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
            .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };

        esp_err_t ret = adc_oneshot_new_unit(&init_config, &_adc1_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(POWER_TAG, "ADC 初始化失败: %s", esp_err_to_name(ret));
            return false;
        }

        adc_oneshot_chan_cfg_t config = {
            .atten = ADC_ATTEN,
            .bitwidth = ADC_BIT_WIDTH,
        };

        ret = adc_oneshot_config_channel(_adc1_handle, ADC1_CHANNEL, &config);
        if (ret != ESP_OK) {
            ESP_LOGE(POWER_TAG, "ADC 通道配置失败: %s", esp_err_to_name(ret));
            return false;
        }

        ESP_LOGI(POWER_TAG, "电池电压检测初始化完成");
        return true;
    }

    uint32_t getBatMilliVolt()
    {
        if (_adc1_handle == NULL) {
            ESP_LOGW(POWER_TAG, "ADC 未初始化！");
            return 0;
        }

        const int sample_count = 10;
        int32_t raw_sum = 0;

        for (int i = 0; i < sample_count; i++) {
            int raw;
            adc_oneshot_read(_adc1_handle, ADC1_CHANNEL, &raw);
            raw_sum += raw;
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        int32_t raw_avg = raw_sum / sample_count;
        float battery_mv = (raw_avg / 4095.0f) * 11000.0f;
        return (uint32_t)battery_mv;
    }

    uint8_t getBatPercentage()
    {
        uint32_t battery_mv = getBatMilliVolt();

        const uint32_t VOLT_MIN = 3000;
        const uint32_t VOLT_MAX = 4200;

        uint8_t percent = 0;

        if (battery_mv <= VOLT_MIN) {
            percent = 0;
        } else if (battery_mv >= VOLT_MAX) {
            percent = 100;
        } else {
            percent = (uint8_t)((battery_mv - VOLT_MIN) * 100 / (VOLT_MAX - VOLT_MIN));
        }

        // ESP_LOGI(POWER_TAG, "电池电压=%lumV  电量=%d%%", battery_mv, percent);
        return percent;
    }
};

#endif