/**
 * @file AppTypedef.h
 * @author Forairaaaaa
 * @brief 应用程序框架核心类型定义（整个系统的APP接口标准）
 * @details 定义了APP的统一结构体、宏，让所有APP遵守相同规则
 * @version 0.1
 * @date 2023-03-16
 * @copyright Copyright (c) 2023
 */

// 防止头文件被重复包含（标准保护）
#pragma once

// 用于支持 std::string 字符串
#include <string>

// 所有APP都放在 App 命名空间，避免名字冲突
namespace App {

    /**
     * @brief 【APP 标准接口结构体】
     * 这是**整个系统最重要的结构体**
     * 规定：所有APP必须实现这6个函数
     * 相当于“APP标准说明书”
     */
    struct AppRegister_t {

        // 函数指针：返回APP的名字（字符串）
        std::string (*appName)();

        // 函数指针：返回APP的图标（LVGL图片）
        void* (*appIcon)();

        // 函数指针：APP创建时调用（初始化）
        void (*onCreate)();

        // 函数指针：APP主循环（反复运行）
        void (*onLoop)();

        // 函数指针：APP销毁时调用（退出、清理）
        void (*onDestroy)();

        // 函数指针：把硬件设备（屏幕、RTC、WiFi等）传给APP
        void (*getBsp)(void* bsp);
    };

}

/**
 * @brief 【宏：声明一个标准APP】
 * 作用：自动帮你生成一个 AppRegister_t 结构体实例
 * 把你的 APP 函数绑定到标准接口上
 *
 * 例：App_Declare(Settings)
 * 自动生成：
 * static AppRegister_t App_Settings = {
 *     appName: App_Settings_appName,
 *     appIcon: App_Settings_appIcon,
 *     ...
 * };
 */
#define App_Declare(app_name) \
    static AppRegister_t App_ ## app_name = { \
        appName:    App_ ## app_name ## _appName, \
        appIcon:    App_ ## app_name ## _appIcon, \
        onCreate:   App_ ## app_name ## _onCreate, \
        onLoop:     App_ ## app_name ## _onLoop, \
        onDestroy:  App_ ## app_name ## _onDestroy, \
        getBsp:     App_ ## app_name ## _getBsp, \
    };

/**
 * @brief 【宏：注册APP到系统列表】
 * 作用：把你声明好的APP放进全局注册表
 * 让桌面启动器可以找到并启动它
 *
 * 例：App_Login(Settings)
 * 展开就是：App_Settings
 */
#define App_Login(app_name) \
    App_ ## app_name