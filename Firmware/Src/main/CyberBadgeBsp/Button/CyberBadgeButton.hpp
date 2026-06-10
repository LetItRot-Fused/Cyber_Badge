/**
 * @file CyberBadgeButton.hpp
 * @brief 赛博吧唧按键驱动
 * @version 1.0.0
 * @date 2026-05-26
 * @copyright Copyright (c) 2026 LetItRot
 */

#ifndef CYBERBADGE_BUTTON_HPP
#define CYBERBADGE_BUTTON_HPP

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 简单毫秒计时
static inline uint32_t millis(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

class CyberBadgeButton
{
public:
    // 构造函数：引脚 + 消抖时间(默认100ms)
    CyberBadgeButton(uint8_t pin, uint16_t debounce_ms = 100)
        : _pin((gpio_num_t)pin)
        , _delay(debounce_ms)
        , _state(RELEASED)
        , _ignore_until(0)
        , _has_changed(false)
    {
    }

    // 初始化：上拉输入
    void begin()
    {
        gpio_reset_pin(_pin);
        gpio_set_direction(_pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(_pin, GPIO_PULLUP_ONLY);
    }

    // 读取按键状态（必须循环调用）
    bool read()
    {
        if (_ignore_until > millis())
        {
            return _state;
        }

        bool current = gpio_get_level(_pin);
        if (current != _state)
        {
            _ignore_until = millis() + _delay;
            _state = current;
            _has_changed = true;
        }

        return _state;
    }

    // 状态是否翻转
    bool toggled()
    {
        read();
        return has_changed();
    }

    // 是否刚刚按下
    bool pressed()
    {
        return (read() == PRESSED && has_changed());
    }

    // 是否刚刚释放
    bool released()
    {
        return (read() == RELEASED && has_changed());
    }

    // 状态是否改变过（自动清零）
    bool has_changed()
    {
        bool temp = _has_changed;
        _has_changed = false;
        return temp;
    }

    // 按键状态定义（与原库一致）
    const static bool PRESSED  = false;
    const static bool RELEASED = true;

private:
    gpio_num_t  _pin;
    uint16_t    _delay;
    bool        _state;
    uint32_t    _ignore_until;
    bool        _has_changed;
};

#endif