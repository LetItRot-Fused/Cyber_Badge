#if 1
#pragma once
#include "../AppTypedef.h"
#include "../../CyberBadgeUIConfigs.h"

/**
 * @brief Create an App in name space 
 * 
 */
namespace App {

    std::string App_Gongde_appName();
    void* App_Gongde_appIcon();
    void App_Gongde_onCreate();
    void App_Gongde_onLoop();
    void App_Gongde_onDestroy();
    void App_Gongde_getBsp(void* bsp);

    /**
     * @brief Declace your App like this 
     * 
     */
    App_Declare(Gongde);
}

#endif
