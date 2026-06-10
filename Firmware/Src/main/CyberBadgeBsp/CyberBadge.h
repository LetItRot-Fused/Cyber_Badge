/**
 * @file CyberBadge.h
 * @author LetItRot
 * @brief
 * @version 1.0.0
 * @date 2026-05-26
 * @copyright Copyright (c) 2026 LetItRot
 */

#pragma once
#include <string>
#include "Button/CyberBadgeButton.hpp"
#include "Power/CyberBadgePower.hpp"
#include "SD/CyberBadgeSD.hpp"
#include "LCD/CyberBadgeLCD.hpp"
#include "RTC/CyberBadgeRTC.hpp"
#include "NVS/CyberBadgeNVS.hpp"

// 版本信息
#define CYBERBADGE_VERSION "V1.0.0"
#define CYBERBADGE_PROJECT "CyberBadge"
#define AUTHOR "LetItRot"

class CyberBadge
{
public:
    // 开机 LOGO
    const std::string Logo = R"LOGO(
 ____   __   ____   ___  ____ 
(  _ \ / _\ (    \ / __)(  __)
 ) _ (/    \ ) D (( (_ \ ) _) 
(____/\_/\_/(____/ \___/(____) 
)LOGO";

    // 硬件对象
    CyberBadgeSD SD;
    CyberBadgePower Power;
    CyberBadgeButton ButtonA{GPIO_NUM_2, 50};
    CyberBadgeButton ButtonB{GPIO_NUM_41, 50};
    CyberBadgeLCD Lcd;
    CyberBadgeRTC RTC;
    CyberBadgeNVS NVS;

public:
    // 启动函数
    void init();
};
