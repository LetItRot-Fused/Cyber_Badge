/**
 * @file App_2048.cpp
 * @author LetItRot
 * @brief 2048小游戏（纯AI写的 我只改了ui的样式）
 * @version 1.0.0
 * @date 2026-05-26
 * @copyright Copyright (c) 2026 LetItRot
 */

#if 1
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "App_2048.h"
#include "../../../CyberBadgeBsp/CyberBadge.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <cstdlib>

static std::string app_name = "2048";
static CyberBadge* device = nullptr;

LV_IMG_DECLARE(ui_img_icon_2048_png);

#define SIZE 4
static uint32_t board[SIZE][SIZE] = {0};
static lv_obj_t* containers[SIZE][SIZE];  // 格子容器
static lv_obj_t* labels[SIZE][SIZE];       // 容器内的数字标签
static lv_obj_t *root_screen = nullptr;

void add_random_tile();
void draw_board();
bool slide_row_left(uint32_t row[]);
bool game_move_left();
bool game_move_right();
bool game_move_up();
bool game_move_down();

void gesture_event_cb(lv_event_t* e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    bool moved = false;

    if (dir == LV_DIR_LEFT)  moved = game_move_left();
    if (dir == LV_DIR_RIGHT) moved = game_move_right();
    if (dir == LV_DIR_TOP)   moved = game_move_up();
    if (dir == LV_DIR_BOTTOM) moved = game_move_down();

    if (moved) {
        add_random_tile();
        draw_board();
    }
}

namespace App {

    std::string App_2048_appName() { return app_name; }
    void* App_2048_appIcon() { return (void*)&ui_img_icon_2048_png; }

    void App_2048_onCreate() {
        UI_LOG("[%s] onCreate\n", App_2048_appName().c_str());

        root_screen = lv_obj_create(nullptr);
        lv_obj_set_size(root_screen, 360, 360);
        lv_obj_set_style_bg_color(root_screen, lv_color_hex(0x414141), 0);  // 灰色背景
        lv_scr_load(root_screen);

        // 创建格子容器 + 内部 label
        for (int y = 0; y < SIZE; y++) {
            for (int x = 0; x < SIZE; x++) {
                // 1. 创建容器（相当于面板）
                containers[y][x] = lv_obj_create(root_screen);
                lv_obj_set_size(containers[y][x], 57, 57);
                lv_obj_set_pos(containers[y][x], 54 + x * 63, 54 + y * 63);
                
                // 容器样式：背景、边框、圆角
                lv_obj_set_style_bg_opa(containers[y][x], LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(containers[y][x], 2, 0);
                lv_obj_set_style_radius(containers[y][x], 20, 0);
                // 清空默认内边距，否则 flex 居中会受影响
                lv_obj_set_style_pad_all(containers[y][x], 0, 0);
                
                // 开启 flex 布局，让内部 label 自动居中
                lv_obj_set_flex_flow(containers[y][x], LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(containers[y][x], 
                                      LV_FLEX_ALIGN_CENTER,   // 主轴（水平）居中
                                      LV_FLEX_ALIGN_CENTER,   // 交叉轴（垂直）居中
                                      LV_FLEX_ALIGN_CENTER);  // 无额外间距

                // 2. 在容器内创建 label
                labels[y][x] = lv_label_create(containers[y][x]);
                lv_label_set_text(labels[y][x], "");
                lv_obj_set_style_text_font(labels[y][x], &lv_font_montserrat_14, 0);
                // label 背景透明，让容器背景显示出来
                lv_obj_set_style_bg_opa(labels[y][x], LV_OPA_TRANSP, 0);
            }
        }

        memset(board, 0, sizeof(board));
        add_random_tile();
        add_random_tile();
        draw_board();

        lv_obj_add_event_cb(root_screen, gesture_event_cb, LV_EVENT_GESTURE, nullptr);
    }

    void App_2048_onLoop() {
        if (device->ButtonB.pressed()) {
            UI_LOG("exit 2048\n");
            return;
        }
    }

    void App_2048_onDestroy() {
        UI_LOG("[%s] onDestroy\n", App_2048_appName().c_str());
        if (root_screen) {
            lv_obj_del_async(root_screen);
            root_screen = nullptr;
        }
    }

    void App_2048_getBsp(void* bsp) {
        device = (CyberBadge*)bsp;
    }
}

// ---------- 游戏逻辑 ----------
void add_random_tile() {
    int empty_x[SIZE*SIZE], empty_y[SIZE*SIZE];
    int cnt = 0;

    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            if (board[y][x] == 0) {
                empty_x[cnt] = x;
                empty_y[cnt] = y;
                cnt++;
            }
        }
    }

    if (cnt == 0) return;
    int r = rand() % cnt;
    board[empty_y[r]][empty_x[r]] = (rand() % 100 < 80) ? 2 : 4;
}

void draw_board() {
    char buf[16];
    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            // 设置容器背景色（方块颜色）
            lv_color_t color;
            if (board[y][x] == 0) {
                color = lv_color_hex(0xE3FDFF);
                lv_label_set_text(labels[y][x], "");   // 空格不显示数字
            } else {
                snprintf(buf, sizeof(buf), "%lu", (unsigned long)board[y][x]);
                lv_label_set_text(labels[y][x], buf);
                switch (board[y][x]) {
                    case 2:    color = lv_color_hex(0xFFCFFD); break;
                    case 4:    color = lv_color_hex(0xBCB2FF); break;
                    case 8:    color = lv_color_hex(0xAAFEFF); break;
                    case 16:   color = lv_color_hex(0x94FFA4); break;
                    case 32:   color = lv_color_hex(0xFFFB97); break;
                    case 64:   color = lv_color_hex(0xFFB076); break;
                    case 128:  color = lv_color_hex(0xFF7B7B); break;
                    case 256:  color = lv_color_hex(0xFF7929); break;
                    case 512:  color = lv_color_hex(0xFF2F8D); break;
                    case 1024: color = lv_color_hex(0xFF064C); break;
                    case 2048: color = lv_color_hex(0xFF1414); break;
                    case 4096: color = lv_color_hex(0xA70606); break;
                    case 8192: color = lv_color_hex(0x47FF00); break;
                    default:   color = lv_color_hex(0xE3FDFF); break;
                }
            }
            lv_obj_set_style_bg_color(containers[y][x], color, 0);
        }
    }
}

bool slide_row_left(uint32_t row[]) {
    uint32_t temp[SIZE] = {0};
    int idx = 0;
    bool moved = false;

    for (int i = 0; i < SIZE; i++)
        if (row[i] != 0) temp[idx++] = row[i];

    for (int i = 0; i < SIZE - 1; i++) {
        if (temp[i] == temp[i+1] && temp[i] != 0) {
            temp[i] *= 2;
            temp[i+1] = 0;
            moved = true;
        }
    }

    idx = 0;
    uint32_t fin[SIZE] = {0};
    for (int i = 0; i < SIZE; i++)
        if (temp[i] != 0) fin[idx++] = temp[i];

    if (memcmp(row, fin, sizeof(fin)) != 0)
        moved = true;

    memcpy(row, fin, sizeof(fin));
    return moved;
}

bool game_move_left() {
    bool ok = false;
    for (int y = 0; y < SIZE; y++)
        if (slide_row_left(board[y])) ok = true;
    return ok;
}

bool game_move_right() {
    bool ok = false;
    for (int y = 0; y < SIZE; y++) {
        uint32_t rev[SIZE];
        for (int i = 0; i < SIZE; i++) rev[i] = board[y][SIZE-1-i];
        if (slide_row_left(rev)) ok = true;
        for (int i = 0; i < SIZE; i++) board[y][SIZE-1-i] = rev[i];
    }
    return ok;
}

bool game_move_up() {
    bool ok = false;
    for (int x = 0; x < SIZE; x++) {
        uint32_t col[SIZE];
        for (int y = 0; y < SIZE; y++) col[y] = board[y][x];
        if (slide_row_left(col)) ok = true;
        for (int y = 0; y < SIZE; y++) board[y][x] = col[y];
    }
    return ok;
}

bool game_move_down() {
    bool ok = false;
    for (int x = 0; x < SIZE; x++) {
        uint32_t col[SIZE];
        for (int y = 0; y < SIZE; y++) col[y] = board[SIZE-1-y][x];
        if (slide_row_left(col)) ok = true;
        for (int y = 0; y < SIZE; y++) board[SIZE-1-y][x] = col[y];
    }
    return ok;
}

#endif