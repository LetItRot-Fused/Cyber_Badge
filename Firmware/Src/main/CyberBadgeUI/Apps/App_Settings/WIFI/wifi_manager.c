#include "wifi_manager.h"
#include <stdio.h>
#include "esp_log.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_heap_caps.h"

#include "nvs_flash.h"
#include "nvs.h"

#define TAG "wifi_manager"

#define NVS_NAMESPACE "wifi_cfg"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PWD "password"

// 重连次数
#define MAX_CONNECT_RETRY 6
static int sta_connect_count = 0;

// 回调函数
static p_wifi_state_callback wifi_state_cb = NULL;

// 当前sta连接状态
static bool is_sta_connected = false;
static bool is_wifi_init = false;

/** 事件回调函数
 * @param arg   用户传递的参数
 * @param event_base    事件类别
 * @param event_id      事件ID
 * @param event_data    事件携带的数据
 * @return 无
 */
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Connected to AP");
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            if (is_sta_connected)
            {
                if (wifi_state_cb)
                    wifi_state_cb(WIFI_STATE_DISCONNECTED); // 通知断开
                is_sta_connected = false;
            }

            // 重连逻辑
            if (sta_connect_count < MAX_CONNECT_RETRY)
            {
                wifi_mode_t mode;
                esp_wifi_get_mode(&mode);
                if (mode == WIFI_MODE_STA)
                    esp_wifi_connect();
                sta_connect_count++;
            }
            // 重连次数用完 → 通知连接失败
            else
            {
                ESP_LOGI(TAG, "Connect failed, max retry reached");
                if (wifi_state_cb)
                    wifi_state_cb(WIFI_STATE_DISCONNECTED);
                sta_connect_count = 0;
            }
            break;

        default:
            break;
        }
    }

    if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        // 成功获取IP → 通知连接成功
        case IP_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "Get ip address");
            is_sta_connected = true;
            sta_connect_count = 0;

            // 这里通知 UI：连接成功
            if (wifi_state_cb)
                wifi_state_cb(WIFI_STATE_CONNECTED);
            break;

        default:
            break;
        }
    }
}

/** 初始化wifi，默认进入STA模式
 * @param 无
 * @return 无
 */
void wifi_manager_init(void)
{
    if (!is_wifi_init)
    {
        // ============= 只在第一次初始化时创建 =============
        static bool sys_init = false;
        if (!sys_init) {
            ESP_ERROR_CHECK(esp_netif_init());
            esp_event_loop_create_default();
            esp_netif_create_default_wifi_sta();
            sys_init = true;
        }

        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        // 注册回调
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        is_wifi_init = true;
        ESP_LOGI(TAG, "wifi_init finished.");
    }
}

static SemaphoreHandle_t scan_sem = NULL;

void wifi_driver_deinit(void)
{
    if(is_wifi_init){
    wifi_manager_set_state_cb(NULL);
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    is_sta_connected = false;
    is_wifi_init = false;
    ESP_LOGI(TAG, "WiFi驱动已反初始化，内存已释放");
    }
    ESP_LOGI(TAG, "WiFi驱动未初始，不需要释放内存");
}

/** 扫描任务
 * @param 无
 * @return 成功/失败
 */
static void scan_task(void *param)
{
    p_wifi_scan_callback callback = (p_wifi_scan_callback)param;
    uint16_t number = 20;
    wifi_ap_record_t *ap_info = malloc(sizeof(wifi_ap_record_t) * number);

    if (ap_info == NULL)
    {
        ESP_LOGE(TAG, "malloc failed");
        xSemaphoreGive(scan_sem);
        vTaskDelete(NULL);
        return;
    }

    uint16_t ap_count = 0;
    ESP_LOGI(TAG, "Start wifi scan");
    if (esp_wifi_scan_start(NULL, true) == ESP_OK)
    {
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
        ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
        if (callback)
            callback(number, ap_info);
    }
    else
    {
        // 新增：扫描失败，手动释放内存
        free(ap_info);
    }
    xSemaphoreGive(scan_sem);
    vTaskDelete(NULL);
}

/** 启动扫描
 * @param 无
 * @return 成功/失败
 */
esp_err_t wifi_manager_scan(p_wifi_scan_callback f)
{
    ESP_LOGI(TAG, "WiFi 扫描准备");
    if (!scan_sem)
    {
        ESP_LOGI(TAG, "!scan_sem");
        scan_sem = xSemaphoreCreateBinary();
        xSemaphoreGive(scan_sem);
    }
    if (pdTRUE == xSemaphoreTake(scan_sem, 100))
    {
        ESP_LOGI(TAG, "scan_sem");
        // 清除上次的扫描信息
        esp_wifi_clear_ap_list();
        // 启动一个扫描任务
        ESP_LOGI(TAG, "WiFi 扫描任务准备开始");
        if (pdTRUE == xTaskCreatePinnedToCore(scan_task, "scan", 4096, f, 5, NULL, 0))
        ESP_LOGI(TAG, "WiFi 扫描任务成功开始");
            return ESP_OK;
    }
        else
        {
            ESP_LOGI(TAG, "WiFi 扫描任务失败");
            // 新增：任务创建失败，归还信号量
            xSemaphoreGive(scan_sem);
            return ESP_FAIL;
        }
    }
    

    /** 连接wifi
     * @param ssid
     * @param password
     * @return 成功/失败
     */
    esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    // 打印即将使用的 SSID 和密码（注意密码仅在调试时打印）
    ESP_LOGI(TAG, "Attempting to connect to SSID: '%s', Password: '%s'", ssid ? ssid : "(null)", password ? password : "(null)");

    sta_connect_count = 0;
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    snprintf((char *)wifi_config.sta.ssid, 31, "%s", ssid);
    snprintf((char *)wifi_config.sta.password, 63, "%s", password);

    // 可选：再次打印从配置中读取的值，确保 snprintf 正确复制
    ESP_LOGI(TAG, "Configured SSID: '%s', Password: '%s'", wifi_config.sta.ssid, wifi_config.sta.password);

    ESP_ERROR_CHECK(esp_wifi_disconnect());
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode != WIFI_MODE_STA)
    {
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        esp_wifi_start();
    }
    else
    {
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        esp_wifi_connect();
    }
    return ESP_OK;
}

    /** 是否已经连接了路由器
     * @param 无
     * @return 是/否
     */
    bool wifi_manager_is_connect(void)
    {
        return is_sta_connected;
    }

void wifi_manager_set_state_cb(p_wifi_state_callback cb)
{
    wifi_state_cb = cb;
}
    // 保存 WiFi 到 NVS
    esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password)
    {
        nvs_handle_t nvs;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "NVS open failed");
            return err;
        }

        nvs_set_str(nvs, NVS_KEY_SSID, ssid);
        nvs_set_str(nvs, NVS_KEY_PWD, password);
        nvs_commit(nvs);
        nvs_close(nvs);

        ESP_LOGI(TAG, "WiFi 已保存: %s", ssid);
        return ESP_OK;
    }

    // 从 NVS 读取 WiFi
    esp_err_t wifi_manager_load_credentials(char *ssid, char *password, size_t ssid_len, size_t pwd_len)
    {
        nvs_handle_t nvs;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);

        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "No saved WiFi");
            return err;
        }

        size_t len = ssid_len;
        nvs_get_str(nvs, NVS_KEY_SSID, ssid, &len);
        len = pwd_len;
        nvs_get_str(nvs, NVS_KEY_PWD, password, &len);

        nvs_close(nvs);

        if (strlen(ssid) == 0)
        {
            return ESP_ERR_NVS_NOT_FOUND;
        }

        ESP_LOGI(TAG, "读取到保存的 WiFi: %s", ssid);
        return ESP_OK;
    }

    // 自动连接上次保存的WiFi
    esp_err_t wifi_manager_auto_connect(void)
    {
        if (!is_wifi_init) {
        wifi_manager_init();
    }
    
        char ssid[32] = {0};
        char pass[64] = {0};

        if (wifi_manager_load_credentials(ssid, pass, sizeof(ssid), sizeof(pass)) == ESP_OK)
        {
            ESP_LOGI(TAG, "自动连接 WiFi: %s", ssid);
            return wifi_manager_connect(ssid, pass);
        }

        ESP_LOGW(TAG, "无保存的WiFi，无法自动连接");
        return ESP_ERR_NOT_FOUND;
    }