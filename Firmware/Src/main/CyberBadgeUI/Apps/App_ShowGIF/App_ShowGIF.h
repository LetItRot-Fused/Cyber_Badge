#if 1
#pragma once
#include "../AppTypedef.h"
#include "../../CyberBadgeUIConfigs.h"

/**
 * @brief Create an App in name space 
 * 
 */
namespace App {

    std::string App_ShowGIF_appName();
    void* App_ShowGIF_appIcon();
    void App_ShowGIF_onCreate();
    void App_ShowGIF_onLoop();
    void App_ShowGIF_onDestroy();
    void App_ShowGIF_getBsp(void* bsp);

    /**
     * @brief Declace your App like this 
     * 
     */
    App_Declare(ShowGIF);
}

#endif
