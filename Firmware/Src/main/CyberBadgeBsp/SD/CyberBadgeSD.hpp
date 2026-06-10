/**
 * @file CyberBadgeSD.hpp
 * @author LetItRot
 * @brief 赛博吧唧 SD卡 SPI3 驱动（纯ESP-IDF原生，无Arduino）
 * @version 1.0.0
 * @date 2026-05-26
 * @copyright Copyright (c) 2026 LetItRot
 */
#pragma once    // 防止头文件被重复包含（很重要，避免编译报错）

// 以下都是 ESP-IDF 自带的官方库，不需要额外下载
#include <stdio.h>                  // 标准输入输出，用于 fopen/fprintf 文件操作
#include "freertos/FreeRTOS.h"     // FreeRTOS 系统内核（ESP-IDF 基于它）
#include "driver/gpio.h"           // GPIO 驱动（控制 CS 引脚）
#include "driver/spi_master.h"     // SPI 主机驱动（和 SD 卡通信）
#include "esp_log.h"               // 日志打印（ESP_LOGI、ESP_LOGE）
#include "esp_vfs_fat.h"           // 虚拟文件系统 + FATFS（让SD卡变成文件夹）
#include "sdmmc_cmd.h"             // SD/MMC 卡底层通信命令（官方核心）
#include "driver/sdspi_host.h"     // SD SPI 模式主机驱动

// ==============================
// 硬件引脚配置（根据你的板子定义）
// ==============================
#define SDCARD_HOST         SPI3_HOST       // 使用 SPI3 控制器（VSPI）
#define PIN_MISO            GPIO_NUM_6      // SD卡 MISO 引脚
#define PIN_MOSI            GPIO_NUM_5      // SD卡 MOSI 引脚
#define PIN_CLK             GPIO_NUM_7      // SD卡 时钟引脚
#define PIN_CS              GPIO_NUM_4      // SD卡 片选引脚
#define MOUNT_POINT         "/sdcard"       // 挂载路径：SD卡会变成这个路径

static const char *SDTAG = "CyberBadgeSD";    // 日志TAG，打印时用来区分来源

// ==============================
// SD卡驱动类
// ==============================
class CyberBadgeSD {
private:
    bool _inited = false;                   // 初始化状态：true=成功，false=未初始化
    static sdmmc_card_t *s_card;            // 静态指针：保存SD卡信息（容量、类型、速度等）

public:
    // 构造函数：创建对象时自动把初始化状态设为 false
    inline CyberBadgeSD() : _inited(false) {}
    // 析构函数：对象销毁时什么也不做（这里不需要释放资源）
    inline ~CyberBadgeSD() {}

    /**
     * @brief 初始化SD卡（SPI3模式）
     * @return bool 成功返回true，失败返回false
     */
    bool init() {
        // ---------------------
        // 1. 创建SD卡SPI主机配置（使用官方默认配置）
        // ---------------------
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.max_freq_khz = 20000;          // 设置SPI速度：20MHz（稳定）
        host.slot = SDCARD_HOST;            // 绑定到 SPI3

        // ---------------------
        // 2. SPI总线硬件配置（引脚、功能全部设置）
        // ---------------------
        spi_bus_config_t bus_cfg = {
            .mosi_io_num = PIN_MOSI,        // 主机发数据引脚
            .miso_io_num = PIN_MISO,        // 主机收数据引脚
            .sclk_io_num = PIN_CLK,         // 时钟引脚
            .quadwp_io_num = -1,            // 四线模式不用，设为-1
            .quadhd_io_num = -1,            // 四线模式不用，设为-1
            .data4_io_num = -1,             // 不用
            .data5_io_num = -1,             // 不用
            .data6_io_num = -1,             // 不用
            .data7_io_num = -1,             // 不用
            .data_io_default_level = 0,     // 空闲电平默认0
            .max_transfer_sz = 4096,        // 最大传输大小4096字节（标准）
            .flags = 0,                     // 无特殊标志
            .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO, // 中断自动分配CPU
            .intr_flags = 0,                // 中断无特殊标志
        };

        // ---------------------
        // 3. 初始化SPI总线（启动硬件）
        // ---------------------
        // 把SPI控制器、引脚配置、DMA通道全部激活
        esp_err_t ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) {               // 如果初始化失败
            ESP_LOGE(SDTAG, "SD卡SPI安装失败: %s", esp_err_to_name(ret)); // 打印错误
            return false;                  // 返回失败，退出
        }

        // ---------------------
        // 4. SD卡设备配置（CS引脚 + 绑定SPI主机）
        // ---------------------
        sdspi_device_config_t dev_cfg = SDSPI_DEVICE_CONFIG_DEFAULT(); // 使用官方默认配置
        dev_cfg.gpio_cs = PIN_CS;                                      // 设置SD卡的片选引脚GPIO4
        dev_cfg.host_id = (spi_host_device_t)host.slot;                // 告诉SD卡用SPI3通信

        // ---------------------
        // 5. 文件系统挂载配置
        // ---------------------
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = true,    // 如果挂载失败，自动格式化SD卡
            .max_files = 5,                    // 最大同时打开5个文件
            .allocation_unit_size = 1024,      // 最小存储单元1KB（平衡速度与空间）
        };

        // ---------------------
        // 6. 挂载SD卡（让系统识别SD卡为 /sdcard 目录）
        // ---------------------
        ret = esp_vfs_fat_sdspi_mount(
            MOUNT_POINT,   // 挂载路径：/sdcard
            &host,         // SPI主机配置
            &dev_cfg,      // SD卡设备（CS引脚）
            &mount_config, // 文件系统设置
            &s_card        // 输出：把卡信息存到 s_card
        );

        // 如果挂载失败（没插卡、坏卡、接触不良）
        if (ret != ESP_OK) {
            ESP_LOGE(SDTAG, "SD卡挂载失败: %s", esp_err_to_name(ret)); // 打印错误信息
            spi_bus_free((spi_host_device_t)host.slot);             // 释放SPI资源，防止占用
            return false;                                           // 返回失败
        }

        // ---------------------
        // 7. 初始化成功
        // ---------------------
        _inited = true;                                 // 标记初始化完成
        ESP_LOGI(SDTAG, "SD 卡挂载成功");                 // 打印成功日志
        sdmmc_card_print_info(stdout, s_card);          // 打印SD卡信息（容量、类型、速度）
        return true;                                    // 返回成功
    }
};

// 类的静态成员必须在类外初始化（C++固定语法）
inline sdmmc_card_t* CyberBadgeSD::s_card = nullptr;