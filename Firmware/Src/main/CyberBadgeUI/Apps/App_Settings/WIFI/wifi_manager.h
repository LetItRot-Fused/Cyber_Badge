#ifndef _WIFI_MANAGER_H_
#define _WIFI_MANAGER_H_

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus          // ⬅️ 新增：加上 extern "C" 保护
extern "C" {
#endif

typedef enum
{
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTED,
} wifi_state_t;

//扫描完成回调函数
typedef void(*p_wifi_scan_callback)(int numbers, wifi_ap_record_t *ap_records);

//wifi状态变化回调函数
typedef void(*p_wifi_state_callback)(wifi_state_t state);

/** 初始化wifi，默认进入STA模式
 * @param f wifi状态变化回调函数
 * @return 无 
*/
void wifi_manager_init();
void wifi_driver_deinit();
/** 启动扫描
 * @param 无
 * @return 成功/失败
*/
esp_err_t wifi_manager_scan(p_wifi_scan_callback f);

/** 连接wifi
 * @param ssid
 * @param password
 * @return 成功/失败
*/
esp_err_t wifi_manager_connect(const char* ssid, const char* password);

/** 是否已经连接了路由器
 * @param 无
 * @return 是/否
*/
bool wifi_manager_is_connect(void);
void wifi_manager_set_state_cb(p_wifi_state_callback cb); // 这个函数
/** 保存ssid，password
 * @param ssid
 * @param password
 * @return 是/否
*/
esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password);  // ⬅️ 加上了分号

// ⬇️ 新增两个函数声明（如果你在 .c 中实现了，就加上，否则可省略）
esp_err_t wifi_manager_load_credentials(char *ssid, char *password, size_t ssid_len, size_t pwd_len);
esp_err_t wifi_manager_auto_connect(void);

#ifdef __cplusplus          
}
#endif

#endif