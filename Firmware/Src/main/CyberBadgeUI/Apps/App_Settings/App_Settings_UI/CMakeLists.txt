SET(SOURCES screens/ui_settingAPP.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_img_wifi_png.c
    images/ui_img_img_time_png.c
    images/ui_img_img_ble_png.c
    images/ui_img_img_res_png.c
    images/ui_img_img_ok_png.c
    fonts/ui_font_text20.c
    fonts/ui_font_wendu42.c
    fonts/ui_font_zhongwen30.c)

add_library(ui ${SOURCES})
