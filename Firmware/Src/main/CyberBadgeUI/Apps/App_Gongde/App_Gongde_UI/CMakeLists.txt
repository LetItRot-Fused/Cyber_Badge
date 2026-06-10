SET(SOURCES screens/ui_gongdeAPP.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_gongde01_png.c
    images/ui_img_gongde02_png.c
    fonts/ui_font_text20.c
    fonts/ui_font_wendu42.c
    fonts/ui_font_zhongwen30.c)

add_library(ui ${SOURCES})
