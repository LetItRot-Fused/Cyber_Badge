/**
 * @file App_Gongde.cpp
 * @author LetItRot
 * @brief 电子木鱼，使用SquareLine Studio制作的
 * @version 1.0.0
 * @date 2026-05-26
 * @copyright Copyright (c) 2026 LetItRot
 */

#if 1
#include "App_Gongde.h"
#include "../../../CyberBadgeBsp/CyberBadge.h"
#include "App_Gongde_UI/ui.h"




static std::string app_name = "Gongde";
static CyberBadge* device;



LV_IMG_DECLARE(ui_img_icon_gongde_png);
namespace App {
    std::string App_Gongde_appName()
    {
        return app_name;
    }
    void* App_Gongde_appIcon()
    {
        return (void*)&ui_img_icon_gongde_png;
    }

    void App_Gongde_onCreate()
    {
        UI_LOG("[%s] onCreate\n", App_Gongde_appName().c_str());
        AppGongde_ui_init();
        // 加这一行！让 LVGL 把画面刷新完成，避免中途销毁
    lv_task_handler();
    }

    void App_Gongde_onLoop()
    {

    }

    void App_Gongde_onDestroy()
    {
        UI_LOG("[%s] onDestroy\n", App_Gongde_appName().c_str());
        ui_gongdeAPP_screen_destroy();
    }
    void App_Gongde_getBsp(void* bsp)
    {
        device = (CyberBadge*)bsp;
    }
    
}

#endif
