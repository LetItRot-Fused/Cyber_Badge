/**
 * @file App_ShowGIF.cpp
 * @author LetItRot
 * @brief 显示 GIF / JPG。自动扫描 SD 卡(/sdcard)里所有 jpg/jpeg/gif 并轮播，
 *        左右滑动切换。图片可通过「手机传图」App 上传
 * @version 2.0.0
 * @date 2026-06-08
 * @copyright Copyright (c) 2026 LetItRot
 */

#if 1
#include "App_ShowGIF.h"
#include "../../../CyberBadgeBsp/CyberBadge.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <cctype>
#include <dirent.h>

static std::string app_name = "ShowGIF";
static CyberBadge* device;

LV_IMG_DECLARE(ui_img_icon_ShowGif_png);

#define SD_MOUNT     "/sdcard"   // POSIX 路径(fopen 用)
#define LV_DRIVE     "S:"        // LVGL 文件系统盘符(映射到 /sdcard)
#define MAX_IMAGES   64
#define MAX_NAME     128

struct ImgEntry {
    char name[MAX_NAME];   // 纯文件名，如 badge1.jpg
    bool is_gif;
};

lv_obj_t *ui_ShowGIFAPP = NULL;

// GIF 相关
static lv_obj_t *gif_obj = NULL;
static uint8_t *gif_buf = NULL;
static size_t gif_size = 0;
static lv_img_dsc_t *saved_img_dsc = NULL;

// JPG 相关
static lv_obj_t *badge_img = NULL;

// 空提示
static lv_obj_t *empty_label = NULL;

// 扫描到的图片列表
static ImgEntry img_list[MAX_IMAGES];
static int img_count = 0;
static int current_index = -1;

// ------------------------------
// 判断后缀，返回 0=非图片 1=jpg 2=gif
// ------------------------------
static int classify_ext(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    char ext[8] = {0};
    size_t n = strlen(dot);
    if (n >= sizeof(ext)) return 0;
    for (size_t i = 0; i < n; i++) ext[i] = (char)tolower((unsigned char)dot[i]);
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return 1;
    if (strcmp(ext, ".gif") == 0) return 2;
    return 0;
}
// ------------------------------
// 生成给 LVGL 用的路径，并把扩展名转成小写
// 例如 BADGE1.JPG -> S:/BADGE1.jpg
// ------------------------------
static void make_lvgl_img_path(const char *name, char *out, size_t out_size)
{
    snprintf(out, out_size, "S:/%s", name);

    char *dot = strrchr(out, '.');
    if (dot) {
        for (char *p = dot; *p; ++p) {
            *p = (char)tolower((unsigned char)*p);
        }
    }
}
// ------------------------------
// 扫描 SD 卡，填充 img_list
// ------------------------------
static void scan_images(void)
{
    img_count = 0;
    DIR *dir = opendir(SD_MOUNT);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && img_count < MAX_IMAGES) {
        int kind = classify_ext(ent->d_name);
        if (kind == 0) continue;
        if (strlen(ent->d_name) >= MAX_NAME) continue;
        strncpy(img_list[img_count].name, ent->d_name, MAX_NAME - 1);
        img_list[img_count].name[MAX_NAME - 1] = 0;
        img_list[img_count].is_gif = (kind == 2);
        img_count++;
    }
    closedir(dir);

    // 打印扫描到的图片列表
printf("\n==================== 扫描到图片 ====================\n");
printf("总计：%d 张\n", img_count);
for (int i = 0; i < img_count; i++) {
    printf("[%d] %s  (%s)\n",
           i,
           img_list[i].name,
           img_list[i].is_gif ? "GIF" : "JPG");
}
printf("=====================================================\n\n");

}

// ------------------------------
// GIF 加载到内存(返回新 buffer，不动旧的)
// ------------------------------
static uint8_t *read_gif_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    uint8_t *buf = (uint8_t*)malloc(sz);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, sz, f);
    fclose(f);
    *out_size = (size_t)sz;
    return buf;
}

// ------------------------------
// 显示当前索引的图片
// ------------------------------
static void show_current(void)
{
    if (img_count == 0 || current_index < 0) {
        if (gif_obj)   lv_obj_add_flag(gif_obj, LV_OBJ_FLAG_HIDDEN);
        if (badge_img) lv_obj_add_flag(badge_img, LV_OBJ_FLAG_HIDDEN);
        if (empty_label) lv_obj_clear_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (empty_label) lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);

    ImgEntry *e = &img_list[current_index];

    if (e->is_gif) {
        char path[160];
        snprintf(path, sizeof(path), "%s/%s", SD_MOUNT, e->name);

        size_t new_size = 0;
        uint8_t *new_buf = read_gif_file(path, &new_size);
        if (new_buf) {
            if (!gif_obj) {
                gif_obj = lv_gif_create(ui_ShowGIFAPP);
                lv_obj_center(gif_obj);
            }
            if (!saved_img_dsc) {
                saved_img_dsc = (lv_img_dsc_t*)lv_mem_alloc(sizeof(lv_img_dsc_t));
            }
            memset(saved_img_dsc, 0, sizeof(lv_img_dsc_t));
            saved_img_dsc->data = new_buf;
            saved_img_dsc->data_size = new_size;
            lv_gif_set_src(gif_obj, saved_img_dsc);

            // 切到新 buffer 后再释放旧 buffer(旧的此前被解码器引用)
            if (gif_buf) free(gif_buf);
            gif_buf = new_buf;
            gif_size = new_size;

            lv_obj_clear_flag(gif_obj, LV_OBJ_FLAG_HIDDEN);
            if (badge_img) lv_obj_add_flag(badge_img, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        // 读取失败则当作无图处理
    } else {
        if (!badge_img) {
            badge_img = lv_img_create(ui_ShowGIFAPP);
            lv_obj_center(badge_img);
        }

        char lvpath[160];
        make_lvgl_img_path(e->name, lvpath, sizeof(lvpath));

        // 调试打印
        char test_path[256];
        snprintf(test_path, sizeof(test_path), "%s/%s", SD_MOUNT, e->name);
        FILE* ft = fopen(test_path, "rb");
        if (ft) {
            printf("系统文件可读: %s\n", test_path);
            fclose(ft);
        } else {
            printf("系统文件打开失败: %s\n", test_path);
        }
        printf("LVGL加载路径(规范化后): %s\n", lvpath);

        lv_img_set_src(badge_img, lvpath);
        lv_obj_center(badge_img);
        lv_obj_clear_flag(badge_img, LV_OBJ_FLAG_HIDDEN);
        if (gif_obj) lv_obj_add_flag(gif_obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void on_screen_loaded(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_current_target(e);
    if (target != ui_ShowGIFAPP) return;

    extern void lv_split_jpeg_init(void);
    lv_split_jpeg_init();

    scan_images();
    current_index = (img_count > 0) ? 0 : -1;
    show_current();
}

// ------------------------------
// 手势：左右切换(循环)
// ------------------------------
void ui_event_ShowGIFAPP(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE)
        return;

    lv_indev_t *indev = lv_indev_get_act();
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    lv_indev_wait_release(indev);

    if (img_count == 0) return;

    if (dir == LV_DIR_LEFT) {
        current_index = (current_index + 1) % img_count;
        show_current();
    } else if (dir == LV_DIR_RIGHT) {
        current_index = (current_index - 1 + img_count) % img_count;
        show_current();
    }
}


void ui_ShowGIFAPP_screen_init(void)
{
    ui_ShowGIFAPP = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ShowGIFAPP, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_ShowGIFAPP, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ShowGIFAPP, LV_OPA_COVER, LV_PART_MAIN);

    // 无图片时的提示(默认隐藏)
    empty_label = lv_label_create(ui_ShowGIFAPP);
    lv_label_set_text(empty_label, "No image\nUse phone to upload");
    lv_obj_set_style_text_align(empty_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(empty_label, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_center(empty_label);
    lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(ui_ShowGIFAPP, on_screen_loaded, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(ui_ShowGIFAPP, ui_event_ShowGIFAPP, LV_EVENT_ALL, NULL);
}

void ui_ShowGIFAPP_screen_destroy(void)
{
    if (gif_buf) { free(gif_buf); gif_buf = NULL; }
    if (saved_img_dsc) { lv_mem_free(saved_img_dsc); saved_img_dsc = NULL; }

    if (ui_ShowGIFAPP) {
        lv_obj_del_async(ui_ShowGIFAPP);
        ui_ShowGIFAPP = NULL;
    }

    gif_obj = NULL;
    badge_img = NULL;
    empty_label = NULL;
    img_count = 0;
    current_index = -1;
}


namespace App {
    std::string App_ShowGIF_appName(){return app_name;}
    void* App_ShowGIF_appIcon()
    {
        return (void*)&ui_img_icon_ShowGif_png;
    }

    void App_ShowGIF_onCreate() {
        UI_LOG("[%s] onCreate\n", app_name.c_str());
        ui_ShowGIFAPP_screen_init();
        lv_scr_load(ui_ShowGIFAPP);
    }

    void App_ShowGIF_onLoop() {}

    void App_ShowGIF_onDestroy() {
        UI_LOG("[%s] onDestroy\n", app_name.c_str());
        ui_ShowGIFAPP_screen_destroy();
    }

    void App_ShowGIF_getBsp(void* bsp) {
        device = (CyberBadge*)bsp;
    }
}
#endif
