/**
 * @file badge_softap.c
 * @brief 赛博吧唧 WiFi 热点(AP)模式启停
 */
#include "badge_softap.h"
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

#define TAG "badge_softap"
// 这些系统级初始化全局只能做一次，用 static 守护（和 wifi_manager 同样思路）
static bool s_sys_inited = false;
static esp_netif_t *s_ap_netif = NULL;
static bool s_running = false;

static void ap_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base != WIFI_EVENT) return;
    if (id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *e = (wifi_event_ap_staconnected_t *)data;
        ESP_LOGI(TAG, "手机已连接, aid=%d", e->aid);
    } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *e = (wifi_event_ap_stadisconnected_t *)data;
        ESP_LOGI(TAG, "手机已断开, aid=%d", e->aid);
    }
}

esp_err_t badge_softap_start(void)
{
    if (s_running) {
        ESP_LOGW(TAG, "热点已在运行");
        return ESP_OK;
    }

    // 系统级初始化只做一次(NVS 由 BSP 启动时已初始化)
    if (!s_sys_inited) {
        ESP_ERROR_CHECK(esp_netif_init());
        // event loop 可能已被 wifi_manager 创建，忽略 already-created 错误
        esp_err_t loop_ret = esp_event_loop_create_default();
        if (loop_ret != ESP_OK && loop_ret != ESP_ERR_INVALID_STATE) {
            ESP_ERROR_CHECK(loop_ret);
        }
        s_sys_inited = true;
    }

    if (!s_ap_netif) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &ap_event_handler, NULL));

    wifi_config_t ap_cfg = {0};
    strncpy((char *)ap_cfg.ap.ssid, BADGE_AP_SSID, sizeof(ap_cfg.ap.ssid));
    ap_cfg.ap.ssid_len = strlen(BADGE_AP_SSID);
    ap_cfg.ap.channel = 1;
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.beacon_interval = 100;

    if (strlen(BADGE_AP_PASSWORD) >= 8) {
        strncpy((char *)ap_cfg.ap.password, BADGE_AP_PASSWORD, sizeof(ap_cfg.ap.password));
        ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        // 密码不足 8 位 → 开放热点
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_running = true;
    ESP_LOGI(TAG, "热点已开启 SSID:%s 密码:%s IP:%s",
             BADGE_AP_SSID, BADGE_AP_PASSWORD, BADGE_AP_IP);
    return ESP_OK;
}

void badge_softap_stop(void)
{
    if (!s_running) return;

    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &ap_event_handler);
    esp_wifi_stop();
    esp_wifi_deinit();

    if (s_ap_netif) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
    }

    s_running = false;
    ESP_LOGI(TAG, "热点已关闭");
}

bool badge_softap_is_running(void)
{
    return s_running;
}
