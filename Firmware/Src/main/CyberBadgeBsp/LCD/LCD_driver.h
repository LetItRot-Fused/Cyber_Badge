/**
 * @file lvgl_driver.h
 * @brief LVGL 显示与触摸驱动 (ST77916 + CST816S) 对外接口
 */

#ifndef LVGL_DRIVER_H
#define LVGL_DRIVER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 屏幕分辨率宏（供外部使用） */
#define LVGL_DRIVER_H_RES        (360)
#define LVGL_DRIVER_V_RES        (360)



/**
 * @brief 初始化 LCD 硬件（ST77916）
 * 
 * 配置 QSPI 总线、面板驱动等。初始化完成后屏幕处于点亮状态（背光默认关闭）。
 * 注意：该函数不包含 LVGL 初始化，需要与 lvgl_test() 配合使用。
 */
void LCD_init(void);

/**
 * @brief 初始化触摸硬件（CST816S）
 * 
 * 配置 I2C 总线、触摸芯片中断等。
 */
void touch_init(void);


/**
 * @brief 设置 LCD 背光亮度（PWM 控制）
 * @param duty_percent 亮度百分比，范围 0～100
 */
void backlight_set_brightness(uint8_t duty_percent);



#ifdef __cplusplus
}
#endif

#endif /* LVGL_DRIVER_H */