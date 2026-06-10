#if 1
#pragma once
#include "../AppTypedef.h"
#include "../../CyberBadgeUIConfigs.h"

namespace App {

    std::string App_2048_appName();
    void* App_2048_appIcon();
    void App_2048_onCreate();
    void App_2048_onLoop();
    void App_2048_onDestroy();
    void App_2048_getBsp(void* bsp);

    App_Declare(2048);
}

#endif