/**
 * @file App_Settings.cpp
 * @author LetItRot
 * @brief 设置APP，使用SquareLine Studio制作
 * @version 1.0.0
 * @date 2026-05-26
 * @copyright Copyright (c) 2026 LetItRot
 */


#if 1
#include "App_Settings.h"
#include "../../../CyberBadgeBsp/CyberBadge.h"
#include "esp_wifi.h"
#include "esp_http_client.h"   // HTTP 客户端相关（esp_http_client_*）
#include "esp_log.h"

#include "WIFI/wifi_manager.h"
#include "App_Settings_UI/ui.h"


// HTTPClient http;    
static RTC_TimeTypeDef rtc_time;
static RTC_DateTypeDef rtc_date;

static std::string app_name = "Settings";
static CyberBadge* device;

static const char *TAG = "App_Settings";

LV_IMG_DECLARE(ui_img_icon_setting_png);


namespace App {

    /**
     * @brief Return the App name laucnher, which will be show on launcher App list
     * 
     * @return std::string 
     */
    std::string App_Settings_appName()
    {
        return app_name;
    }


    /**
     * @brief Return the App Icon laucnher, NULL for default
     * 
     * @return void* 
     */
    void* App_Settings_appIcon()
    {
        // return NULL;
        return (void*)&ui_img_icon_setting_png;
    }


    /**
     * @brief Called when App is on create
     * 
     */
    void App_Settings_onCreate()
    {
        UI_LOG("[%s] onCreate1\n", App_Settings_appName().c_str());
        
        APPSettings_ui_init();
    }


    /**
     * @brief Called repeatedly, end this function ASAP! or the App management will be affected
     * If the thing you want to do takes time, try create a taak or lvgl timer to handle them.
     * Try use millis() instead of delay() here
     * 
     */
    void App_Settings_onLoop()
    {

    }


    /**
     * @brief Called when App is about to be destroy
     * Please remember to release the resourse like lvgl timers in this function
     * 
     */
    void App_Settings_onDestroy()
    {
        UI_LOG("[%s] onDestroy\n", App_Settings_appName().c_str());
        APPSettings_ui_destroy();
    }


    /**
     * @brief Launcher will pass the BSP pointer through this function before onCreate
     * 
     */
    void App_Settings_getBsp(void* bsp)
{
    device = (CyberBadge*)bsp;
}
    
}

#endif


