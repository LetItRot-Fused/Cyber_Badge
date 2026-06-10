/**
 * @file lvgl_driver.c
 * @brief 基于 ST77916 LCD + CST816S 触摸的 LVGL 显示与输入驱动
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st77916.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_err.h"
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "driver/ledc.h"

#define EXAMPLE_LCD_H_RES (360)
#define EXAMPLE_LCD_V_RES (360)
#define EXAMPLE_LCD_BIT_PER_PIXEL (16)

#define EXAMPLE_LCD_HOST (SPI2_HOST)
#define EXAMPLE_PIN_NUM_LCD_CS (14)    //
#define EXAMPLE_PIN_NUM_LCD_PCLK (13)  //
#define EXAMPLE_PIN_NUM_LCD_DATA0 (11) //
#define EXAMPLE_PIN_NUM_LCD_DATA1 (12) //
#define EXAMPLE_PIN_NUM_LCD_DATA2 (10) //
#define EXAMPLE_PIN_NUM_LCD_DATA3 (9)  //
#define EXAMPLE_PIN_NUM_LCD_RST (18)   //
#define EXAMPLE_PIN_NUM_LCD_TE (3)     //
#define EXAMPLE_PIN_NUM_BK_LIGHT (8)   //

#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL (1)
// 背光PWM配置
#define BSP_LCD_BACKLIGHT_FREQ_HZ (5000)         // 5kHz
#define BSP_LCD_BACKLIGHT_RES (LEDC_TIMER_8_BIT) // 8位分辨率（0-255）

#define EXAMPLE_TOUCH_HOST (I2C_NUM_0)
#define EXAMPLE_PIN_NUM_TOUCH_SCL (GPIO_NUM_15)
#define EXAMPLE_PIN_NUM_TOUCH_SDA (GPIO_NUM_16)
#define EXAMPLE_PIN_NUM_TOUCH_RST (GPIO_NUM_NC) // Not used
#define EXAMPLE_PIN_NUM_TOUCH_INT (GPIO_NUM_17)

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2

#define EXAMPLE_LVGL_BUFFER_SIZE EXAMPLE_LCD_H_RES * 90 * sizeof(lv_color_t)


static const char *TAG = "Lvgl_driver";

static SemaphoreHandle_t touch_mux = NULL;

static lv_disp_draw_buf_t disp_buf; /* Contains internal graphic buffer(s) called draw buffer(s) */
static lv_disp_drv_t disp_drv;      /* Contains callback functions */
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_touch_handle_t tp = NULL;

static const st77916_lcd_init_cmd_t lcd_init_cmds[] = {
    // 命令字, 参数数据指针, 参数长度, 延时(ms)
    {0xF0, (uint8_t[]){0x28}, 1, 0},
    {0xF2, (uint8_t[]){0x28}, 1, 0},
    {0x73, (uint8_t[]){0xF0}, 1, 0},

    {0x7C, (uint8_t[]){0xD1}, 1, 0},
    {0x83, (uint8_t[]){0xE0}, 1, 0},
    {0x84, (uint8_t[]){0x61}, 1, 0},
    {0xF2, (uint8_t[]){0x82}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xF0, (uint8_t[]){0x01}, 1, 0},
    {0xF1, (uint8_t[]){0x01}, 1, 0},

    {0xB0, (uint8_t[]){0x56}, 1, 0},
    {0xB1, (uint8_t[]){0x4D}, 1, 0},
    {0xB2, (uint8_t[]){0x24}, 1, 0},
    {0xB4, (uint8_t[]){0x87}, 1, 0},
    {0xB5, (uint8_t[]){0x44}, 1, 0},
    {0xB6, (uint8_t[]){0x8B}, 1, 0},
    {0xB7, (uint8_t[]){0x40}, 1, 0},
    {0xB8, (uint8_t[]){0x86}, 1, 0},
    {0xBA, (uint8_t[]){0x00}, 1, 0},
    {0xBB, (uint8_t[]){0x08}, 1, 0},
    {0xBC, (uint8_t[]){0x08}, 1, 0},
    {0xBD, (uint8_t[]){0x00}, 1, 0},

    {0xC0, (uint8_t[]){0x80}, 1, 0},
    {0xC1, (uint8_t[]){0x10}, 1, 0},
    {0xC2, (uint8_t[]){0x37}, 1, 0},
    {0xC3, (uint8_t[]){0x80}, 1, 0},
    {0xC4, (uint8_t[]){0x10}, 1, 0},
    {0xC5, (uint8_t[]){0x37}, 1, 0},
    {0xC6, (uint8_t[]){0xA9}, 1, 0},
    {0xC7, (uint8_t[]){0x41}, 1, 0},
    {0xC8, (uint8_t[]){0x01}, 1, 0},
    {0xC9, (uint8_t[]){0xA9}, 1, 0},
    {0xCA, (uint8_t[]){0x41}, 1, 0},
    {0xCB, (uint8_t[]){0x01}, 1, 0},

    {0xD0, (uint8_t[]){0x91}, 1, 0},
    {0xD1, (uint8_t[]){0x68}, 1, 0},
    {0xD2, (uint8_t[]){0x68}, 1, 0},

    {0xF5, (uint8_t[]){0x00, 0xA5}, 2, 0},

    {0xDD, (uint8_t[]){0x4F}, 1, 0},
    {0xDE, (uint8_t[]){0x4F}, 1, 0},
    {0xF1, (uint8_t[]){0x10}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xF0, (uint8_t[]){0x02}, 1, 0},

    {0xE0, (uint8_t[]){0xF0, 0x0A, 0x10, 0x09, 0x09, 0x36, 0x35, 0x33, 0x4A, 0x29, 0x15, 0x15, 0x2E, 0x34}, 14, 0},
    {0xE1, (uint8_t[]){0xF0, 0x0A, 0x0F, 0x08, 0x08, 0x05, 0x34, 0x33, 0x4A, 0x39, 0x15, 0x15, 0x2D, 0x33}, 14, 0},

    {0xF0, (uint8_t[]){0x10}, 1, 0},
    {0xF3, (uint8_t[]){0x10}, 1, 0},

    {0xE0, (uint8_t[]){0x07}, 1, 0},
    {0xE1, (uint8_t[]){0x00}, 1, 0},
    {0xE2, (uint8_t[]){0x00}, 1, 0},
    {0xE3, (uint8_t[]){0x00}, 1, 0},
    {0xE4, (uint8_t[]){0xE0}, 1, 0},
    {0xE5, (uint8_t[]){0x06}, 1, 0},
    {0xE6, (uint8_t[]){0x21}, 1, 0},
    {0xE7, (uint8_t[]){0x01}, 1, 0},
    {0xE8, (uint8_t[]){0x05}, 1, 0},
    {0xE9, (uint8_t[]){0x02}, 1, 0},
    {0xEA, (uint8_t[]){0xDA}, 1, 0},
    {0xEB, (uint8_t[]){0x00}, 1, 0},
    {0xEC, (uint8_t[]){0x00}, 1, 0},
    {0xED, (uint8_t[]){0x0F}, 1, 0},
    {0xEE, (uint8_t[]){0x00}, 1, 0},
    {0xEF, (uint8_t[]){0x00}, 1, 0},

    {0xF8, (uint8_t[]){0x00}, 1, 0},
    {0xF9, (uint8_t[]){0x00}, 1, 0},
    {0xFA, (uint8_t[]){0x00}, 1, 0},
    {0xFB, (uint8_t[]){0x00}, 1, 0},
    {0xFC, (uint8_t[]){0x00}, 1, 0},
    {0xFD, (uint8_t[]){0x00}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xFF, (uint8_t[]){0x00}, 1, 0},

    {0x60, (uint8_t[]){0x40}, 1, 0},
    {0x61, (uint8_t[]){0x04}, 1, 0},
    {0x62, (uint8_t[]){0x00}, 1, 0},
    {0x63, (uint8_t[]){0x42}, 1, 0},
    {0x64, (uint8_t[]){0xD9}, 1, 0},
    {0x65, (uint8_t[]){0x00}, 1, 0},
    {0x66, (uint8_t[]){0x00}, 1, 0},
    {0x67, (uint8_t[]){0x00}, 1, 0},
    {0x68, (uint8_t[]){0x00}, 1, 0},
    {0x69, (uint8_t[]){0x00}, 1, 0},
    {0x6A, (uint8_t[]){0x00}, 1, 0},
    {0x6B, (uint8_t[]){0x00}, 1, 0},

    {0x70, (uint8_t[]){0x40}, 1, 0},
    {0x71, (uint8_t[]){0x03}, 1, 0},
    {0x72, (uint8_t[]){0x00}, 1, 0},
    {0x73, (uint8_t[]){0x42}, 1, 0},
    {0x74, (uint8_t[]){0xD8}, 1, 0},
    {0x75, (uint8_t[]){0x00}, 1, 0},
    {0x76, (uint8_t[]){0x00}, 1, 0},
    {0x77, (uint8_t[]){0x00}, 1, 0},
    {0x78, (uint8_t[]){0x00}, 1, 0},
    {0x79, (uint8_t[]){0x00}, 1, 0},
    {0x7A, (uint8_t[]){0x00}, 1, 0},
    {0x7B, (uint8_t[]){0x00}, 1, 0},

    {0x80, (uint8_t[]){0x48}, 1, 0},
    {0x81, (uint8_t[]){0x00}, 1, 0},
    {0x82, (uint8_t[]){0x06}, 1, 0},
    {0x83, (uint8_t[]){0x02}, 1, 0},
    {0x84, (uint8_t[]){0xD6}, 1, 0},
    {0x85, (uint8_t[]){0x04}, 1, 0},
    {0x86, (uint8_t[]){0x00}, 1, 0},
    {0x87, (uint8_t[]){0x00}, 1, 0},
    {0x88, (uint8_t[]){0x48}, 1, 0},
    {0x89, (uint8_t[]){0x00}, 1, 0},
    {0x8A, (uint8_t[]){0x08}, 1, 0},
    {0x8B, (uint8_t[]){0x02}, 1, 0},
    {0x8C, (uint8_t[]){0xD8}, 1, 0},
    {0x8D, (uint8_t[]){0x04}, 1, 0},
    {0x8E, (uint8_t[]){0x00}, 1, 0},
    {0x8F, (uint8_t[]){0x00}, 1, 0},

    {0x90, (uint8_t[]){0x48}, 1, 0},
    {0x91, (uint8_t[]){0x00}, 1, 0},
    {0x92, (uint8_t[]){0x0A}, 1, 0},
    {0x93, (uint8_t[]){0x02}, 1, 0},
    {0x94, (uint8_t[]){0xDA}, 1, 0},
    {0x95, (uint8_t[]){0x04}, 1, 0},
    {0x96, (uint8_t[]){0x00}, 1, 0},
    {0x97, (uint8_t[]){0x00}, 1, 0},
    {0x98, (uint8_t[]){0x48}, 1, 0},
    {0x99, (uint8_t[]){0x00}, 1, 0},
    {0x9A, (uint8_t[]){0x0C}, 1, 0},
    {0x9B, (uint8_t[]){0x02}, 1, 0},
    {0x9C, (uint8_t[]){0xDC}, 1, 0},
    {0x9D, (uint8_t[]){0x04}, 1, 0},
    {0x9E, (uint8_t[]){0x00}, 1, 0},
    {0x9F, (uint8_t[]){0x00}, 1, 0},

    {0xA0, (uint8_t[]){0x48}, 1, 0},
    {0xA1, (uint8_t[]){0x00}, 1, 0},
    {0xA2, (uint8_t[]){0x05}, 1, 0},
    {0xA3, (uint8_t[]){0x02}, 1, 0},
    {0xA4, (uint8_t[]){0xD5}, 1, 0},
    {0xA5, (uint8_t[]){0x04}, 1, 0},
    {0xA6, (uint8_t[]){0x00}, 1, 0},
    {0xA7, (uint8_t[]){0x00}, 1, 0},
    {0xA8, (uint8_t[]){0x48}, 1, 0},
    {0xA9, (uint8_t[]){0x00}, 1, 0},
    {0xAA, (uint8_t[]){0x07}, 1, 0},
    {0xAB, (uint8_t[]){0x02}, 1, 0},
    {0xAC, (uint8_t[]){0xD7}, 1, 0},
    {0xAD, (uint8_t[]){0x04}, 1, 0},
    {0xAE, (uint8_t[]){0x00}, 1, 0},
    {0xAF, (uint8_t[]){0x00}, 1, 0},

    {0xB0, (uint8_t[]){0x48}, 1, 0},
    {0xB1, (uint8_t[]){0x00}, 1, 0},
    {0xB2, (uint8_t[]){0x09}, 1, 0},
    {0xB3, (uint8_t[]){0x02}, 1, 0},
    {0xB4, (uint8_t[]){0xD9}, 1, 0},
    {0xB5, (uint8_t[]){0x04}, 1, 0},
    {0xB6, (uint8_t[]){0x00}, 1, 0},
    {0xB7, (uint8_t[]){0x00}, 1, 0},
    {0xB8, (uint8_t[]){0x48}, 1, 0},
    {0xB9, (uint8_t[]){0x00}, 1, 0},
    {0xBA, (uint8_t[]){0x0B}, 1, 0},
    {0xBB, (uint8_t[]){0x02}, 1, 0},
    {0xBC, (uint8_t[]){0xDB}, 1, 0},
    {0xBD, (uint8_t[]){0x04}, 1, 0},
    {0xBE, (uint8_t[]){0x00}, 1, 0},
    {0xBF, (uint8_t[]){0x00}, 1, 0},

    {0xC0, (uint8_t[]){0x10}, 1, 0},
    {0xC1, (uint8_t[]){0x47}, 1, 0},
    {0xC2, (uint8_t[]){0x56}, 1, 0},
    {0xC3, (uint8_t[]){0x65}, 1, 0},
    {0xC4, (uint8_t[]){0x74}, 1, 0},
    {0xC5, (uint8_t[]){0x88}, 1, 0},
    {0xC6, (uint8_t[]){0x99}, 1, 0},
    {0xC7, (uint8_t[]){0x01}, 1, 0},
    {0xC8, (uint8_t[]){0xBB}, 1, 0},
    {0xC9, (uint8_t[]){0xAA}, 1, 0},

    {0xD0, (uint8_t[]){0x10}, 1, 0},
    {0xD1, (uint8_t[]){0x47}, 1, 0},
    {0xD2, (uint8_t[]){0x56}, 1, 0},
    {0xD3, (uint8_t[]){0x65}, 1, 0},
    {0xD4, (uint8_t[]){0x74}, 1, 0},
    {0xD5, (uint8_t[]){0x88}, 1, 0},
    {0xD6, (uint8_t[]){0x99}, 1, 0},
    {0xD7, (uint8_t[]){0x01}, 1, 0},
    {0xD8, (uint8_t[]){0xBB}, 1, 0},
    {0xD9, (uint8_t[]){0xAA}, 1, 0},

    {0xF3, (uint8_t[]){0x01}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},

    {0x36, (uint8_t[]){0xC0}, 1, 0},
    {0x21, NULL, 0, 0},
    {0x11, NULL, 0, 0},

    {0x00, NULL, 0, 120}, // Delay 120ms

    {0x29, NULL, 0, 0},

    {0x00, NULL, 0, 0} // 结束标记

};

/**
 * @brief 初始化背光PWM（替代 GPIO 简单开关）
 */
static void backlight_pwm_init(void)
{
    ESP_LOGI(TAG, "Init backlight PWM");

    // 配置LEDC定时器（ESP32-S3仅低速模式）
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = BSP_LCD_BACKLIGHT_RES,
        .freq_hz = BSP_LCD_BACKLIGHT_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 配置LEDC通道，绑定到背光引脚
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = EXAMPLE_PIN_NUM_BK_LIGHT,
        .duty = 0, // 初始占空比0
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

/**
 * @brief 设置背光亮度（0～100%）
 */
void backlight_set_brightness(uint8_t duty_percent)
{
    uint32_t duty_max = (1 << BSP_LCD_BACKLIGHT_RES) - 1;
    uint32_t duty = (duty_percent * duty_max) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

//=====================================================  LVGL设置  ================================================
static void example_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    /* Read touch controller data */
    if (xSemaphoreTake(touch_mux, 0) == pdTRUE)
    {
        esp_lcd_touch_read_data(drv->user_data);
    }

    /* Get coordinates */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(drv->user_data, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0)
    {
        data->point.x = 359 - touchpad_x[0];
        data->point.y = 359 - touchpad_y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    /* Copy a buffer's content to a specific area of the display */
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

static void touch_callback(esp_lcd_touch_handle_t tp)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(touch_mux, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}


void LCD_init(void)
{

    backlight_pwm_init();
    backlight_set_brightness(0);          // 先关闭
    touch_mux = xSemaphoreCreateBinary(); /* Define a mutex for the touch */
    xSemaphoreGive(touch_mux);
    // 初始化 SPI 总线
    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = ST77916_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK,
                                                                  EXAMPLE_PIN_NUM_LCD_DATA0,
                                                                  EXAMPLE_PIN_NUM_LCD_DATA1,
                                                                  EXAMPLE_PIN_NUM_LCD_DATA2,
                                                                  EXAMPLE_PIN_NUM_LCD_DATA3,
                                                                  EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * sizeof(uint16_t));

    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 安装屏幕 IO 驱动
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = ST77916_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS,
                                                                                 example_notify_lvgl_flush_ready, &disp_drv);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install ST77916 panel driver");
    const st77916_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds, // Uncomment these line if use custom initialization commands
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(st77916_lcd_init_cmd_t),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,  // Implemented by LCD command `36h`
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL, // Implemented by LCD command `3Ah` (16/18)
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77916(io_handle, &panel_config, &panel_handle));

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);
}
void touch_init(void)
{
    // 初始化I2C总线
    ESP_LOGI(TAG, "Initialize I2C bus");
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = EXAMPLE_PIN_NUM_TOUCH_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = EXAMPLE_PIN_NUM_TOUCH_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(EXAMPLE_TOUCH_HOST, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(EXAMPLE_TOUCH_HOST, I2C_MODE_MASTER, 0, 0, 0));

    // 给触摸芯片上电稳定时间
    vTaskDelay(pdMS_TO_TICKS(200));

    ESP_LOGI(TAG, "Initialize touch components");
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    
    const esp_lcd_panel_io_i2c_config_t tp_io_config = {
        .dev_addr = 0x15,  // CST816S 默认I2C地址
        .control_phase_bytes = 1,  // 控制阶段字节数
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags = {
            .disable_control_phase = 1,  // CST816S不需要控制字节
        }
    };
    
    esp_err_t ret = esp_lcd_new_panel_io_i2c(EXAMPLE_TOUCH_HOST, &tp_io_config, &tp_io_handle);
    if (ret != ESP_OK || tp_io_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create I2C IO for touch: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "I2C IO created successfully");
    
    // 配置触摸参数
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = EXAMPLE_PIN_NUM_TOUCH_RST,
        .int_gpio_num = EXAMPLE_PIN_NUM_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
        .interrupt_callback = touch_callback,
    };
    
    // 创建触摸驱动
    ret = esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create CST816S touch driver: %s", esp_err_to_name(ret));
        return;
    }
    
    if (tp == NULL) {
        ESP_LOGE(TAG, "Touch handle is NULL after creation!");
        return;
    }
    
    ESP_LOGI(TAG, "Touch init successful!");
}

void lvgl_init(void)
{
    /* =============== Initialize LVGL =============== */
    ESP_LOGI(TAG, "Initialize LVGL");

    lv_init();
    /* Alloc draw buffers used by LVGL */
    /* It's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized */
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(EXAMPLE_LVGL_BUFFER_SIZE, MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(EXAMPLE_LVGL_BUFFER_SIZE, MALLOC_CAP_DMA);
    assert(buf2);
    /* Initialize LVGL draw buffers */
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * 90);

    /* =============== Register display driver to LVGL =============== */
    ESP_LOGI(TAG, "Register display driver to LVGL");

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    /* =============== Register touch driver to LVGL =============== */
    ESP_LOGI(TAG, "Register touch driver to LVGL");

    static lv_indev_drv_t indev_drv; /* Input device driver (Touch) */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev_drv.read_cb = example_lvgl_touch_cb;
    indev_drv.user_data = tp;

    lv_indev_drv_register(&indev_drv);

    /* =============== Register touch driver to LVGL =============== */
    ESP_LOGI(TAG, "Install LVGL tick timer");

    /* Tick interface for LVGL (using esp_timer to generate 2ms periodic event) */
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"};
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    // Turn on backlight
#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
#endif
    backlight_set_brightness(80);
}
