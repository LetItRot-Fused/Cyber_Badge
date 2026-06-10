/**
 * @file App_Launcher.h
 * @author LetItRot 改自Forairaaaaa的arduino版
 * @brief 桌面启动器（主屏幕）
 * @version 1.0.0
 * @date 2026-05-26
 * @copyright Copyright (c) 2026 LetItRot
 */
#pragma once
#include "../Apps/AppRegister.h"
#include "../../CyberBadgeBsp/CyberBadge.h"


namespace App {

    class App_Launcher {
        private:
            /* BSP pointer to access hardware functions */
            CyberBadge* _device;

            void updateDeviceStatus();
            void updateAppManage();

            /* UI events */
            lv_timer_t* _time_update_timer;
            lv_timer_t* _weather_fetch_timer;
            static void time_update(lv_timer_t * timer);
            static void weather_update();
            static void weather_fetch_task(lv_timer_t *timer);
            static void scroll_event_cb(lv_event_t * e);
            static void button_event_cb(lv_event_t * e);
            static void panel_control_pad_event_cb(lv_event_t * e);
            static void sleep_mode();

        public:
            inline App_Launcher(CyberBadge* device) {  _device = device; }

            void onCreate();
            void onLoop();
            void onDestroy();

    };

}
