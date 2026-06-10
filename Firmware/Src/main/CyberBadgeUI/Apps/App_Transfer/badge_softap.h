/**
 * @file badge_softap.h
 * @brief 赛博吧唧 WiFi 热点(AP)模式启停，供「手机传图」App 使用
 * @note  独立于 wifi_manager(STA)，进入前请先 wifi_driver_deinit() 释放 STA
 */
#ifndef _BADGE_SOFTAP_H_
#define _BADGE_SOFTAP_H_

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// AP 固定参数（网关默认 192.168.4.1）
#define BADGE_AP_SSID       "CyberBadge"
#define BADGE_AP_PASSWORD   "12345678"     // 留空字符串则开放热点
#define BADGE_AP_IP         "192.168.4.1"

/** 启动 AP 热点
 * @return ESP_OK 成功
 */
esp_err_t badge_softap_start(void);

/** 关闭 AP 热点，释放资源 */
void badge_softap_stop(void);

/** 当前是否已开启热点 */
bool badge_softap_is_running(void);

#ifdef __cplusplus
}
#endif

#endif
