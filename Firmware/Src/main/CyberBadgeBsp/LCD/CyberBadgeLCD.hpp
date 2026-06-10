/**
 * @file CyberBadgeLCD.cpp
 * @author LetItRot
 * @brief 赛博吧唧lvgl的一些自定义函数
 * @version 1.0.0
 * @date 2026-05-26
 * @copyright Copyright (c) 2026 LetItRot
 */

#pragma once

#include "lvgl.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C"
{
#endif

    void backlight_set_brightness(uint8_t duty_percent);
    void LCD_init(void);
    void touch_init(void);
    void lvgl_init(void);

#ifdef __cplusplus
}

class CyberBadgeLCD
{
private:
    bool _enable;
    TaskHandle_t _task_handler;
    SemaphoreHandle_t _semaphore_mutex;
    bool _task_inited;

    /**
     * @brief Task to handle lvgl timer
     *
     * @param param
     */
    static void task_lv_timer_handler(void *param)
    {
        CyberBadgeLCD *lvgl = (CyberBadgeLCD *)param;
        while (1)
        {
            if (lvgl->isEnable())
                lv_timer_handler();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        vTaskDelete(NULL);
    }

public:
    CyberBadgeLCD() : _enable(false), _task_handler(nullptr), _semaphore_mutex(nullptr), _task_inited(false) {}
    ~CyberBadgeLCD() = default;

    /**
     * @brief Init lvgl and create a task to handle lv timer
     */
    inline void init()
    {
        LCD_init();
        touch_init();
        lvgl_init();
    }

    inline bool lv_task_create(const char *task_name, uint32_t stack_size = 8192, UBaseType_t priority = 5) {
    if (_task_inited) {
        ESP_LOGW("LCD", "LVGL task already exists. Call lv_task_delete() first.");
        return false;
    }
    _semaphore_mutex = xSemaphoreCreateMutex();
    _enable = false;
    BaseType_t ret = xTaskCreatePinnedToCore(
    task_lv_timer_handler,task_name,stack_size,this,priority,&_task_handler,1);
    if (ret == pdPASS) {
        _task_inited = true;
        return true;
    } else {
        vSemaphoreDelete(_semaphore_mutex);
        _semaphore_mutex = nullptr;
        return false;
    }
}
inline void lv_task_delete() {
    if (!_task_inited) return;
    _enable = false;
    if (_task_handler) vTaskDelete(_task_handler);
    if (_semaphore_mutex) vSemaphoreDelete(_semaphore_mutex);
    _task_handler = nullptr;
    _semaphore_mutex = nullptr;
    _task_inited = false;
}

inline void lock(TickType_t xTicksToWait = portMAX_DELAY) {
    if (_semaphore_mutex) xSemaphoreTake(_semaphore_mutex, xTicksToWait);
}
inline void unlock() {
    if (_semaphore_mutex) xSemaphoreGive(_semaphore_mutex);
}
    /**
     * @brief Enable lvgl refreshing
     *
     * @param xTicksToWait
     */
    inline void enable(TickType_t xTicksToWait = portMAX_DELAY) {
    if (!_semaphore_mutex) return;
    if (_enable) return;
    xSemaphoreTake(_semaphore_mutex, xTicksToWait);
    _enable = true;
    xSemaphoreGive(_semaphore_mutex);
}
    /**
     * @brief Disable lvgl refreshing, your should calll this before calling lvgl API like lv_lable...
     *
     * @param xTicksToWait
     */
    inline void disable(TickType_t xTicksToWait = portMAX_DELAY) {
    if (!_semaphore_mutex) return;  // 添加这行
    if (!_enable) return;
    xSemaphoreTake(_semaphore_mutex, xTicksToWait);
    _enable = false;
    xSemaphoreGive(_semaphore_mutex);
}

    /**
     * @brief Is lvgl refreshing
     *
     * @param xTicksToWait
     * @return true
     * @return false
     */
    inline bool isEnable(TickType_t xTicksToWait = portMAX_DELAY) {
    if (!_semaphore_mutex) return false;  // 添加这行
    xSemaphoreTake(_semaphore_mutex, xTicksToWait);
    bool ret = _enable;
    xSemaphoreGive(_semaphore_mutex);
    return ret;
}

    void setBrightness(uint8_t percent)
    {
        backlight_set_brightness(percent);
    }

    // ==============================
    // 屏幕创建
    // ==============================
    lv_obj_t *create_screen(uint8_t r, uint8_t g, uint8_t b)
    {
        lv_obj_t *screen = lv_obj_create(NULL);
        lv_obj_remove_style_all(screen);
        lv_obj_set_size(screen, 360, 360);

        lv_color_t bg_color = lv_color_make(r, g, b);
        lv_obj_set_style_bg_color(screen, bg_color, 0);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

        lv_scr_load(screen);
        return screen;
    }
    void destroy_screen(lv_obj_t* screen)
{
    if (screen == NULL) return;

    disable();        
    lv_obj_del(screen);
    enable();
}
    // ==============================
    // 面板创建
    // ==============================
    lv_obj_t *create_panel(lv_obj_t *parent_screen,
                           uint16_t w, uint16_t h,
                           uint8_t r = 255, uint8_t g = 255, uint8_t b = 255)
    {
        lv_obj_t *panel = lv_obj_create(parent_screen);
        lv_obj_set_size(panel, 240, 320); // 面板固定大小（可视区域）

        // 开启滚动功能（核心！）
        lv_obj_add_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        // 可选：只允许垂直滚动（屏蔽水平滚动）
        lv_obj_set_scroll_dir(panel, LV_DIR_VER);

        lv_color_t panel_color = lv_color_make(r, g, b);
        lv_obj_set_style_bg_color(panel, panel_color, 0);
        lv_obj_set_style_radius(panel, 0, 0);
        lv_obj_set_style_border_width(panel, 1, 0);
        lv_obj_set_size(panel, w, h);
        lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);

        lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(panel,
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);

        return panel;
    }

    void lvgl_add_text(lv_obj_t *target_panel, const char *text, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255)
    {
        lock();

        lv_obj_t *label = lv_label_create(target_panel);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
        lv_color_t text_color = lv_color_make(r, g, b);
        lv_obj_set_style_text_color(label, text_color, 0);

        lv_obj_update_layout(target_panel);                  // 刷新布局
        lv_obj_scroll_to_view_recursive(label, LV_ANIM_OFF); // 直接滚到刚创建的 label 位置

        unlock();
    }

void clear(uint16_t color = 0x0000) {
    lock();                                 // 确保线程安全
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_invalidate(scr);                 // 触发重绘
    unlock();
}


};

#endif