/**
 * @file App_Transfer.cpp
 * @brief 手机传图 App：进入后开 WiFi 热点 + HTTP 网页上传服务
 *        手机连热点后用浏览器打开 http://192.168.4.1 即可选图上传到 SD 卡
 * @note  屏幕文字用 ASCII(连接信息本就是英文/数字)，montserrat 字体保证渲染；
 *        中文交互界面在手机网页里(浏览器字体完整)。
 */
#if 1
#include "App_Transfer.h"
#include "lvgl.h"
#include "esp_log.h"

#include "badge_softap.h"
#include "upload_httpd.h"
// 进入前关掉可能在运行的 STA WiFi，避免和 AP 抢资源
#include "../App_Settings/WIFI/wifi_manager.h"

LV_IMG_DECLARE(ui_img_icon_ap_png);
LV_IMG_DECLARE(ui_img_backgroundimg_ap_png);

static std::string app_name = "Transfer";
static void *device = NULL;

static lv_obj_t *ui_TransferAPP = NULL;
static lv_obj_t *status_label = NULL;
static uint32_t last_shown_count = 0xFFFFFFFF;



static lv_obj_t *make_label(lv_obj_t *parent, const char *txt, const lv_font_t *font,
                            lv_color_t color, lv_align_t align, int y)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, color, LV_PART_MAIN);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(l, align, 0, y);
    return l;
}

static void build_ui(void)
{
    ui_TransferAPP = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_TransferAPP, LV_OBJ_FLAG_SCROLLABLE);
     lv_obj_set_style_bg_img_src(ui_TransferAPP, &ui_img_backgroundimg_ap_png, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_TransferAPP, LV_OPA_COVER, LV_PART_MAIN);

    // lv_color_t white = lv_color_hex(0xFFFFFF);
    // lv_color_t blue  = lv_color_hex(0x0A84FF);
    // lv_color_t gray  = lv_color_hex(0xAAAAAA);

    // make_label(ui_TransferAPP, "Phone  ->  Badge", &lv_font_montserrat_22, blue,  LV_ALIGN_TOP_MID, 24);

    // make_label(ui_TransferAPP, "1. Connect WiFi",   &lv_font_montserrat_14, gray,  LV_ALIGN_TOP_MID, 70);
    // make_label(ui_TransferAPP, "SSID: " BADGE_AP_SSID,     &lv_font_montserrat_22, white, LV_ALIGN_TOP_MID, 92);
    // make_label(ui_TransferAPP, "PASS: " BADGE_AP_PASSWORD, &lv_font_montserrat_22, white, LV_ALIGN_TOP_MID, 122);

    // make_label(ui_TransferAPP, "2. Open in browser", &lv_font_montserrat_14, gray,  LV_ALIGN_TOP_MID, 168);
    // make_label(ui_TransferAPP, "http://" BADGE_AP_IP, &lv_font_montserrat_22, blue,  LV_ALIGN_TOP_MID, 190);

    // status_label = make_label(ui_TransferAPP, "Uploaded: 0", &lv_font_montserrat_22, white,
    //                           LV_ALIGN_BOTTOM_MID, -24);
}

namespace App {

    std::string App_Transfer_appName() { return app_name; }

    // 返回 NULL → Launcher 自动使用默认图标
    void* App_Transfer_appIcon() 
    {
        return (void*)&ui_img_icon_ap_png; 
    }

    void App_Transfer_onCreate()
    {
        UI_LOG("[%s] onCreate\n", app_name.c_str());

        build_ui();
        lv_scr_load(ui_TransferAPP);

        // 关掉 STA，再开 AP + HTTP
        wifi_driver_deinit();
        badge_softap_start();
        upload_httpd_start();

        last_shown_count = 0xFFFFFFFF;  // 强制首次刷新
    }

    void App_Transfer_onLoop()
    {
        // // 节流：每秒检查一次上传计数变化再刷新 UI
        // static uint32_t last_tick = 0;
        // uint32_t now = lv_tick_get();
        // if (now - last_tick < 1000) return;
        // last_tick = now;

        // uint32_t cnt = upload_httpd_get_count();
        // if (cnt != last_shown_count && status_label) {
        //     last_shown_count = cnt;
        //     char buf[32];
        //     snprintf(buf, sizeof(buf), "Uploaded: %lu", (unsigned long)cnt);
        //     lv_label_set_text(status_label, buf);
        // }
    }

    void App_Transfer_onDestroy()
    {
        UI_LOG("[%s] onDestroy\n", app_name.c_str());

        upload_httpd_stop();
        badge_softap_stop();

        status_label = NULL;
        if (ui_TransferAPP) {
            lv_obj_del_async(ui_TransferAPP);
            ui_TransferAPP = NULL;
        }
    }

    void App_Transfer_getBsp(void *bsp) { device = bsp; }
}
#endif
