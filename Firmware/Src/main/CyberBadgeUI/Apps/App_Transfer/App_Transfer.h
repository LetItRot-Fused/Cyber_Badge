#if 1
#pragma once
#include "../AppTypedef.h"
#include "../../CyberBadgeUIConfigs.h"

/**
 * @brief 手机传图 App：开 WiFi 热点 + HTTP 网页上传
 */
namespace App {

    std::string App_Transfer_appName();
    void* App_Transfer_appIcon();
    void App_Transfer_onCreate();
    void App_Transfer_onLoop();
    void App_Transfer_onDestroy();
    void App_Transfer_getBsp(void* bsp);

    App_Declare(Transfer);
}

#endif
