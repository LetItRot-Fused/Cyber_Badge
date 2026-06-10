/**
 * @file ChappieUI.h
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-03-15
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#pragma once
#include "CyberBadgeUIConfigs.h"
#include "Launcher/App_Launcher.h"
#include "../CyberBadgeBsp/CyberBadge.h"


class CyberBadgeUI {
    private:
        bool _inited;
        CyberBadge* _device;
        App::App_Launcher* _launcher;

    public:
        CyberBadgeUI() : _inited(false) {}
        ~CyberBadgeUI() {}

        /**
         * @brief UI begin
         * 
         */
        int begin();
        
        /**
         * @brief Put it into loop
         * 
         */
        void update();
        
        /* Get device bsp pointor */
        inline CyberBadge* device() { return _device; }
};


