/**
 * @file main.cpp
 * @author LetItRot
 * @brief 程序入口
 * @version 1.0.0
 * @date 2026-05-26
 * @copyright Copyright (c) 2026 LetItRot
 */

#include <stdio.h>
#include "CyberBadgeUI/CyberBadgeUI.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"



//下面的代码是调试用的
//需要去SDK配置编辑器勾选
//configUSE_TRACE_FACILITY
//configUSE_STATS_FORMATTING_FUNCTIONS

// void print_all_tasks_info(void)
// {
//     // 静态大缓冲区，不占栈
//     static char buf[4096];
//     memset(buf, 0, sizeof(buf));
//     vTaskList(buf);

//     printf("\n=================== System Task List ===================\n");
//     printf("任务名称          状态  优先级  剩余栈   编号 核心\n");
//     printf("--------------------------------------------------------\n");
//     printf("%s", buf);
//     printf("========================================================\n");
// }

// void task_monitor(void *pvParameters)
// {
//     uint32_t delay_ms = (uint32_t)pvParameters;
//     while (1) {
//         print_all_tasks_info();
//         vTaskDelay(pdMS_TO_TICKS(delay_ms));
//     }
// }

// void start_task_list_monitor(uint32_t interval_ms, uint32_t stack_size, UBaseType_t priority)
// {
//     xTaskCreatePinnedToCore(
//         task_monitor,
//         "task_monitor",
//         8192,
//         (void*)interval_ms,
//         priority,
//         NULL,
//         0 
//     );
// }

//我为了刷新率把Flash SPI speed 和 Set RAM clock speed 开到了120Mhz 烧录程序和调试的时候要拔插一下usb low帧可以超过37fps
//如果不稳定可以调回80 并把Default display refresh period (ms).和Input device read period [ms]. 调整到30 锁定最高帧率为33fbs

//RTC使用的是esp32s3自带的RTC 长按关机睡眠状态下误差比较大 建议每次开机去设置里面同步一下时间
//电量检测用的也是esp32s3自带的adc 误差较大只能显示大概的电量 如果需要去调试中查看电量信息 请去这里CyberBadgePower.hpp 取消log注释

CyberBadgeUI badge;

void task_ui_core1(void *pvParam)
{
    badge.begin();
    while (1) {
        badge.update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

extern "C" void app_main(void)
{
    // 所有代码跑core1 避免和wifi抢cpu
    xTaskCreatePinnedToCore(task_ui_core1,"ui_core1",4096,NULL,5,NULL,1);
}