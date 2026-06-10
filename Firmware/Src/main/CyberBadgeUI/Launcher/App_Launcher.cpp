
/**
 * @file App_Launcher.cpp
 * @author LetItRot 改自Forairaaaaa的arduino版
 * @brief 桌面启动器（主屏幕）
 * @version 1.0.0
 * @date 2026-05-26
 * @copyright Copyright (c) 2026 LetItRot
 */

/* 包含头文件：启动器、UI配置、SquareLine生成的UI界面 */
#include "App_Launcher.h"
#include "../CyberBadgeUIConfigs.h"
#include "UI/ui.h"
#include "lvgl.h"
#include "esp_log.h"
#include <time.h>     // time, localtime_r, clock_settime
#include <sys/time.h> // struct timeval
#include "esp_sleep.h"

#include "../Apps/App_Settings/WIFI/wifi_manager.h"
#include "../Apps/App_Settings/HTTP/HTTP.h"



static const char *TAG = "App_Launcher";

/* ========================== 全局状态结构体 ========================== */

/**
 * @brief APP管理结构体
 * 保存所有APP的数量、选中状态、是否正在运行
 */
struct AppManager_t
{
    uint16_t totalNum = 0;  // 总共注册了多少个APP
    int16_t selected = -1;  // 当前选中的APP编号
    bool isRunning = false; // 是否有APP正在运行
    bool onCreate = false;  // 标记：需要创建并打开APP
    bool onDestroy = false; // 标记：需要销毁并退出APP
};
static AppManager_t _app; // 全局APP管理器

/**
 * @brief 设备状态结构体
 * 保存屏幕亮度、自动熄屏等系统状态
 */
struct DeviceStatus_t
{
    bool updated = false;               // 设备状态是否需要更新
    bool autoScreenOff = false;         // 是否开启自动熄屏
    uint8_t brightness = 127;           // 屏幕亮度 0~255
    uint32_t autoScreenOffTime = 20000; // 自动熄屏时间（20秒）
};
static DeviceStatus_t _device_status; // 全局设备状态


/* ========================== 启动器命名空间 ========================== */
namespace App
{

    /**
     * @brief 启动器初始化（进入桌面时调用一次）
     * 作用：初始化UI、创建APP图标、加载时钟、绑定按钮事件
     */
    void App_Launcher::onCreate()
    {

        /* 初始化SquareLine导出的UI */
        ui_init();
        /* 读取注册的APP总数 */
        _app.totalNum = sizeof(App::Register) / sizeof(App::AppRegister_t);

        /* ------------------- 遍历所有APP，自动创建图标 ------------------- */
        for (int i = 0; i < _app.totalNum; i++)
        {
            /* 创建APP图标按钮 */
            lv_obj_t *app_btn = lv_btn_create(ui_TabAPP);
            lv_obj_set_user_data(app_btn, (void *)i);
            lv_obj_set_size(app_btn, 130, 130);
            lv_obj_align(app_btn, LV_ALIGN_CENTER, i * 150, 0);
            lv_obj_add_flag(app_btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
            lv_obj_set_style_radius(app_btn, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(app_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_spread(app_btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

            /* 绑定按钮点击事件 */
            lv_obj_add_event_cb(app_btn, button_event_cb, LV_EVENT_CLICKED, NULL);

            /* 设置APP图标 */
            const lv_img_dsc_t *app_icon = (const lv_img_dsc_t *)App::Register[i].appIcon();
            if (app_icon == NULL)
            {
                app_icon = &ui_img_img_icon_defalut_png;
            }
            lv_obj_set_style_bg_img_src(app_btn, app_icon, LV_PART_MAIN | LV_STATE_DEFAULT);

            /* 创建APP名称标签 */
            // lv_obj_t *app_name = lv_label_create(ui_TabAPP);
            // lv_obj_align(app_name, LV_ALIGN_CENTER, i * 180, 95);
            // lv_label_set_text(app_name, App::Register[i].appName().c_str());
            // lv_obj_set_style_text_color(app_name, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
            // lv_obj_set_style_text_font(app_name, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        /* ------------------- 绑定UI事件回调 ------------------- */
        lv_obj_add_event_cb(ui_TabDesktop, scroll_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(ui_Slider1, panel_control_pad_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(ui_ButtonAutoScreenOff, panel_control_pad_event_cb, LV_EVENT_CLICKED, NULL);
        // lv_obj_add_event_cb(ui_ButtonInfos, panel_control_pad_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(ui_wifiButton, panel_control_pad_event_cb, LV_EVENT_CLICKED, NULL);
        // lv_obj_add_event_cb(ui_ButtonBle, panel_control_pad_event_cb, LV_EVENT_CLICKED, NULL);

        

        /* 回到上次退出的位置 */
        // 开机默认定位到正确的 Tab
        if (_app.selected != -1)
        {
            // 之前打开过 APP → 回到 APP 页面（Tab 2）
            lv_tabview_set_act(ui_TabDesktop, 2, LV_ANIM_OFF);

            // 根据 Tab 自动决定状态栏显示/隐藏
            // APP 页 = 显示状态栏
            lv_obj_set_y(ui_ImgStateBar, -164);
        }
        else
        {
            // 默认进入主页 → Tab 1 (Home)
            lv_tabview_set_act(ui_TabDesktop, 1, LV_ANIM_OFF);

            // Home 页 = 隐藏状态栏
            lv_obj_set_y(ui_ImgStateBar, -198);
        }

        /* 创建定时器：每秒更新时间 */
        _time_update_timer = lv_timer_create(time_update, 1000, (void *)_device);
        time_update(_time_update_timer);
        /* 创建定时器：每1小时自动联网更新天气 */
        _weather_fetch_timer = lv_timer_create(weather_fetch_task, 3600000, NULL);
        /* 重置屏幕无操作计时 */
        lv_disp_trig_activity(NULL);

        /* 更新天气温度 */
        weather_update();
        /* 初始化自动熄屏开关状态 */
        if (_device_status.autoScreenOff)
        {
            lv_obj_add_state(ui_ButtonAutoScreenOff, (LV_STATE_CHECKED | LV_STATE_FOCUSED));
        }
        /* 初始化wifi开关状态 */
        if(wifi_manager_is_connect())
        {
            lv_obj_add_state(ui_wifiButton, LV_STATE_USER_1);
            lv_obj_add_state(ui_wifiButton, LV_STATE_CHECKED);
        }

        // 重新开启LVGL
        _device->Lcd.enable();
    }

    /**
     * @brief 启动器主循环
     * 框架每10~20ms调用一次
     * 作用：更新设备状态 + 管理APP打开/关闭
     */
    void App_Launcher::onLoop()
    {
        // 关闭LVGL防止刷新冲突
        _device->Lcd.disable();

        updateDeviceStatus(); // 更新按键、熄屏、亮度

        updateAppManage(); // 管理APP创建/退出/切换

        // 重新开启LVGL
        _device->Lcd.enable();
    }

    /**
     * @brief 销毁启动器（退出桌面、打开APP时调用）
     * 作用：释放UI资源、删除定时器、清理屏幕
     */
    void App_Launcher::onDestroy()
    {
        /* 1. 删除定时器 */
        if (_time_update_timer)
        {
            lv_timer_del(_time_update_timer);
            _time_update_timer = NULL;
        }
        if (_weather_fetch_timer) {
    lv_timer_del(_weather_fetch_timer);
    _weather_fetch_timer = NULL;
}

        /* 2. 移除所有自定义事件回调（关键！防止删除时触发） */
        if (ui_TabDesktop)
        {
            lv_obj_remove_event_cb(ui_TabDesktop, scroll_event_cb);
        }
        if (ui_Slider1)
        {
            lv_obj_remove_event_cb(ui_Slider1, panel_control_pad_event_cb);
        }
        if (ui_ButtonAutoScreenOff)
        {
            lv_obj_remove_event_cb(ui_ButtonAutoScreenOff, panel_control_pad_event_cb);
        }

        /* 3. 打印删除前的内存（使用ESP堆函数） */
        ESP_LOGI("ui_LaunchScreen_MEM", "Free internal heap before: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        ESP_LOGI("ui_LaunchScreen_MEM", "Largest free block: %d", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

        /* 4. 切换屏幕并删除旧屏幕 */
        lv_obj_t *new_scr = lv_obj_create(NULL);
        lv_disp_load_scr(new_scr);
        if (ui_Launch)
        {
            lv_obj_del(ui_Launch);
            ui_Launch = NULL;
        }

        /* 5. 打印删除后的内存 */
        ESP_LOGI("ui_LaunchScreen_MEM", "Free internal heap after: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }

    /**
     * @brief 更新设备状态
     * 处理：Home键、电源键、自动熄屏、亮度调节
     */
    void App_Launcher::updateDeviceStatus()
    {
        /* 如果按下HOME键 */
        if (_device->ButtonB.pressed())
        {
            if (_app.isRunning)
            {
                // 如果APP正在运行，退出APP
                _app.onDestroy = true;
            }
            else
            {
                // 否则回到桌面第一页
                lv_tabview_set_act(ui_TabDesktop, 1, LV_ANIM_OFF);
            }
            // 重置无操作计时
            lv_disp_trig_activity(NULL);
        }

        /* 如果按下电源键 → 重启 */
        if (_device->ButtonA.pressed())
        {
            ESP_LOGI("KEY", "电源键已按下");

            // 开始计时，等待长按
            uint32_t start_time = millis();

            // 一直按住才继续
            while (gpio_get_level(GPIO_NUM_2) == 0)
            {
                // 长按时间到
                if (millis() - start_time >= 1500)
                {
                    // 等待松开按键
                    while (gpio_get_level(GPIO_NUM_2) == 0)
                    {
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                    ESP_LOGI("KEY", "下次按下 GPIO2 即可唤醒设备");
                    // 配置 GPIO2 低电平唤醒
                    esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 0);
                    // 进入深度睡眠
                    esp_deep_sleep_start();
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            esp_restart();
            ESP_LOGI("KEY", "短按操作，无动作");
        }

        /* 如果亮度等状态需要更新 */
        if (_device_status.updated)
        {
            _device_status.updated = false;
            _device->Lcd.setBrightness(_device_status.brightness);
        }

        /* 自动熄屏检测 */
        if (_device_status.autoScreenOff && (lv_disp_get_inactive_time(NULL) > _device_status.autoScreenOffTime))
        {
            _device->Lcd.setBrightness(0);                         // 熄屏
            sleep_mode();                                          // 进入低功耗睡眠
            _device->Lcd.setBrightness(_device_status.brightness); // 唤醒恢复亮度
            lv_disp_trig_activity(NULL);                           // 重置计时
        }
    }

    /**
     * @brief APP状态管理（核心！）
     * 作用：打开APP、运行APP、关闭APP、返回桌面
     */
    void App_Launcher::updateAppManage()
    {
        /* ------------------- 1. 打开APP ------------------- */
        if (_app.onCreate)
        {
            _app.onCreate = false;
            _app.isRunning = true;

            onDestroy(); // 销毁桌面

            // 打开选中的APP
            App::Register[_app.selected].getBsp(_device);
            App::Register[_app.selected].onCreate();
        }

        /* ------------------- 2. APP运行中 ------------------- */
        if (_app.isRunning)
        {
            App::Register[_app.selected].onLoop();
        }

        /* ------------------- 3. 退出APP，返回桌面 ------------------- */
        if (_app.onDestroy)
        {
            _app.onDestroy = false;
            _app.isRunning = false;

            App::Register[_app.selected].onDestroy(); // 退出APP
            onCreate();                               // 重新创建桌面
        }
    }

    /* ==================================================
     * 以下全是 UI 事件回调函数（时间更新、滑动、按钮点击）
     * ================================================== */

    /**
     * @brief 时钟更新（每秒调用）
     */
    void App_Launcher::time_update(lv_timer_t *timer)
    {
        CyberBadge *device = (CyberBadge *)timer->user_data;

        // 从 ESP32 内置 RTC 读取时间
        RTC_TimeTypeDef rtc_time;
        RTC_DateTypeDef rtc_date;
        device->RTC.getTime(&rtc_time);
        device->RTC.getDate(&rtc_date);
        // 星期对照表（中文）
        const char *week_days[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
        // 格式化时间
        char time_str[32];
        snprintf(time_str, sizeof(time_str), "%02d : %02d", rtc_time.hours, rtc_time.minutes);
        // 格式化日期
        char data_str[32];
        snprintf(data_str, sizeof(data_str), "%02d / %02d", rtc_date.month, rtc_date.date);
        // 格式化星期
        char wday_str[32];
        snprintf(wday_str, sizeof(wday_str), "%s", week_days[rtc_date.weekDay]);
        // 更新UI
        lv_label_set_text(ui_timeLabel, time_str);
        lv_label_set_text(ui_mainTimeLabel, time_str);
        lv_label_set_text(ui_dataLabel, data_str);
        lv_label_set_text(ui_weekdayLabel, wday_str);

        uint8_t percent =  device->Power.getBatPercentage();
        lv_bar_set_value(ui_BatteryBar, percent, LV_ANIM_OFF);

    }


    void App_Launcher::weather_update()
{
    // 安全判断
    if (!lv_obj_is_valid(ui_tempLabel)) return;
    if (!lv_obj_is_valid(ui_weatherLabel2)) return;
        // 2. 必须有天气数据才显示（防止空白）
    if (strlen(weather_temp) == 0 || strlen(weather_text) == 0) {
        ESP_LOGW("Weather", "暂无天气数据，不刷新UI");
        return;
    }

    lv_label_set_text(ui_tempLabel, weather_temp);
    lv_label_set_text(ui_weatherLabel2, weather_text);

    ESP_LOGI("Weather", "UI 天气已刷新");
}
    // 每 1 小时：联网获取天气
void App_Launcher::weather_fetch_task(lv_timer_t *timer)
{
    if (!wifi_manager_is_connect()) {
        return;
    }

    ESP_LOGI("Weather", "自动更新天气...");

    if (location_http_init() == ESP_OK) {
        weather_http_init();
        // 在这里手动刷新 UI（只在成功后刷新）
        weather_update();
    }
}
        






    static void anim_set_y(void *obj, int32_t y)
    {
        if (obj)
            lv_obj_set_y((lv_obj_t *)obj, y);
    }

    void StateBarDropDown_Animation(lv_obj_t *TargetObject, int delay)
    {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, TargetObject);
        lv_anim_set_exec_cb(&a, anim_set_y);
        lv_anim_set_values(&a, -198, -164); // 下拉
        lv_anim_set_time(&a, 500);
        lv_anim_set_delay(&a, delay);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_playback_time(&a, 0);
        lv_anim_set_playback_delay(&a, 0);
        lv_anim_set_repeat_count(&a, 0);
        lv_anim_set_repeat_delay(&a, 0);
        lv_anim_set_early_apply(&a, false);
        lv_anim_start(&a);
    }

    void StateBarPullUp_Animation(lv_obj_t *TargetObject, int delay)
    {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, TargetObject);
        lv_anim_set_exec_cb(&a, anim_set_y);
        lv_anim_set_values(&a, -164, -198); // 上拉
        lv_anim_set_time(&a, 500);
        lv_anim_set_delay(&a, delay);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_playback_time(&a, 0);
        lv_anim_set_playback_delay(&a, 0);
        lv_anim_set_repeat_count(&a, 0);
        lv_anim_set_repeat_delay(&a, 0);
        lv_anim_set_early_apply(&a, false);
        lv_anim_start(&a);
    }
    /**
     * @brief Tabview 切换事件 → 状态栏自动显隐
     * @note  Tab顺序：0=ControlPad，1=Home，2=APP
     *        需求：Home(1)隐藏状态栏，其他显示
     */
    void App_Launcher::scroll_event_cb(lv_event_t *e)
    {
        // 拿到 Tabview 对象
        lv_obj_t *tabview = lv_event_get_target(e);

        // 当前激活 Tab 索引
        uint16_t current_tab = lv_tabview_get_tab_act(tabview);

        // 静态变量：记录上一次 Tab，避免重复动画
        static uint16_t last_tab = 0;

        // 没变就直接返回
        if (current_tab == last_tab)
        {
            return;
        }

        // ---------------- 核心逻辑 ----------------
        if (current_tab == 1)
        {
            // 下拉显示状态栏
            StateBarPullUp_Animation(ui_ImgStateBar, 0);
        }
        else
        {
            // 上拉隐藏状态栏
            StateBarDropDown_Animation(ui_ImgStateBar, 0);
        }
        // -------------------------------------------

        last_tab = current_tab;
    }

    /**
     * @brief APP图标点击事件
     */
    void App_Launcher::button_event_cb(lv_event_t *e)
    {
        lv_obj_t *obj = lv_event_get_target(e);
        lv_event_code_t code = lv_event_get_code(e);

        /* Into that App */
        if (code == LV_EVENT_CLICKED)
        {
            _app.selected = (int)lv_obj_get_user_data(obj);
            _app.onCreate = true;
        }
    }

    static void wifi_status_callback(wifi_state_t state)
    {
        if (state == WIFI_STATE_CONNECTED)
        {
            ESP_LOGI("LAUNCHER", "WiFi 已连接");
            // 亮绿色（你定义的 USER_1 状态）
            lv_obj_add_state(ui_wifiButton, LV_STATE_USER_1);
        }
        else
        {
            ESP_LOGI("LAUNCHER", "WiFi 连接失败");
            // 恢复默认灰色
            lv_obj_clear_state(ui_wifiButton, LV_STATE_USER_1);
            lv_obj_clear_state(ui_wifiButton, LV_STATE_CHECKED);
            wifi_driver_deinit();
        }
    }

    /**
     * @brief 控制面板事件（亮度、自动熄屏、设置等）
     */
    // 控制面板事件
    void App_Launcher::panel_control_pad_event_cb(lv_event_t *e)
    {
        lv_obj_t *obj = lv_event_get_target(e);
        lv_event_code_t code = lv_event_get_code(e);

        // 亮度调节
        if (obj == ui_Slider1)
        {
            _device_status.updated = true;
            _device_status.brightness = lv_slider_get_value(obj);
        }
        // 自动熄屏
        else if (obj == ui_ButtonAutoScreenOff)
        {
            if (lv_obj_get_state(obj) & LV_STATE_CHECKED)
            {
                _device_status.autoScreenOff = true;
                ESP_LOGI(TAG, "自动熄屏已开启");
            }
            else
            {
                _device_status.autoScreenOff = false;
                ESP_LOGI(TAG, "自动熄屏已关闭");
            }
        }

        // ==================== WiFi 按钮 ====================
        else if (obj == ui_wifiButton && code == LV_EVENT_CLICKED)
{
    if (!(lv_obj_get_state(obj) & LV_STATE_CHECKED))
    {
        lv_obj_add_state(ui_wifiButton, LV_STATE_CHECKED);
        wifi_manager_set_state_cb(wifi_status_callback);
        wifi_manager_auto_connect();
    }
    else
    {
        lv_obj_clear_state(ui_wifiButton, LV_STATE_CHECKED);
        lv_obj_clear_state(ui_wifiButton, LV_STATE_USER_1);
        wifi_driver_deinit();
    }
}
    }
    /**
     * @brief 进入低功耗睡眠模式
     */
    void App_Launcher::sleep_mode()
    {
        // 深度睡眠唤醒配置
        esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 0);
        // 等待按键松开
        while (gpio_get_level(GPIO_NUM_2) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        esp_deep_sleep_start();
    }
} // namespace App