/**
 * @file CyberBadgeNVS.hpp
 * @author LetItRot
 * @brief 赛博吧唧NVS初始化
 * @version 1.0.0
 * @date 2026-05-26
 * @copyright Copyright (c) 2026 LetItRot
 */

#pragma once

#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *NVSTAG = "CyberBadgeNVS";

class CyberBadgeNVS {
private:
    bool _inited = false;
public:
    inline CyberBadgeNVS() : _inited(false) {}
    inline ~CyberBadgeNVS() {}

    // ==========================
    // 1. 初始化 NVS
    // ==========================
    bool init() {
        if (_inited) return true;

        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_LOGW(NVSTAG, "NVS 需要擦除重新初始化");
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }

        if (err == ESP_OK) {
            _inited = true;
            ESP_LOGI(NVSTAG, "NVS 初始化成功");
        } else {
            ESP_LOGE(NVSTAG, "NVS 初始化失败: %s", esp_err_to_name(err));
        }

        return _inited;
    }

};