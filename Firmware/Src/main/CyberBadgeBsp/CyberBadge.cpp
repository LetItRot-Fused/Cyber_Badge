/**
 * @file CyberBadge.cpp
 * @author LetItRot
 * @brief  赛博吧唧硬件初始化启动界面
 * @version 1.0.0
 * @date 2026-05-26
 * @copyright Copyright (c) 2026 LetItRot
 */

#include "CyberBadge.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "../CyberBadgeUI/Launcher/UI/ui.h"

void CyberBadge::init()
{
    // 开机打印信息
    Lcd.init();
    lv_obj_t *screen1 = Lcd.create_screen(0, 0, 0);
    lv_obj_t *panel_log = Lcd.create_panel(screen1, 240, 240, 0, 0, 0);
    Lcd.lv_task_create("lv_BOOT_task", 8192, 10);
    Lcd.enable();


    Lcd.lvgl_add_text(panel_log, "BADGELOG:", 0, 255, 0);

    Lcd.lvgl_add_text(panel_log, "LCD Inited");
    Lcd.lvgl_add_text(panel_log, "LVGL Runing");
    Lcd.lvgl_add_text(panel_log, "Touch Runing");

    Lcd.lvgl_add_text(panel_log, "ESP32 DEV Board:", 0, 255, 0);
    Lcd.lvgl_add_text(panel_log, "Install RTC");
    RTC.init();
    Lcd.lvgl_add_text(panel_log, "Install NVS");
    NVS.init();
    Lcd.lvgl_add_text(panel_log, "Hardware Driver:", 0, 255, 0);
    Lcd.lvgl_add_text(panel_log, "Install Power");
    Power.init();
    Lcd.lvgl_add_text(panel_log, "Install Button");
    ButtonA.begin();
    ButtonB.begin();
    Lcd.lvgl_add_text(panel_log, "Install SDcard");
    SD.init();


    vTaskDelay(pdMS_TO_TICKS(200));
    
    Lcd.lvgl_add_text(panel_log, "====SYSTEM BOOT====", 0, 255, 0);
    const char *ascii_art =
        "___      __    ____    ___  ____ \n"
        "(  _ \\   /__\\   (  _  \\  / __)( ___)\n"
        ") _ <  /(__)\\ | (_) )( (_-.  )__) \n"
        "(__./ (_)  (_)(___/ \\__./ (___)\n";

    // 创建标签，设置位置和大小
    lv_obj_t *label = lv_label_create(panel_log);
    lv_label_set_text(label, ascii_art);
    lv_obj_set_style_text_color(label, lv_color_make(0, 255, 0), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0); // 建议换成等宽字体
    // 可选：将标签放在中央
    lv_obj_center(label);
    Lcd.lvgl_add_text(panel_log, "Firmware ByCpp: " CYBERBADGE_VERSION, 0, 255, 0);
    Lcd.lvgl_add_text(panel_log, "Author: " AUTHOR, 0, 255, 0);
    Lcd.lvgl_add_text(panel_log, "Project: " CYBERBADGE_PROJECT, 0, 255, 0);
    Lcd.lvgl_add_text(panel_log, "=====================", 0, 255, 0);

    vTaskDelay(pdMS_TO_TICKS(200));

    // Lcd.lv_task_delete();
    // Lcd.lv_task_create("lv_ui_task",8192,5);

    printf("\n CyberBadge 启动成功！\n\n");
}