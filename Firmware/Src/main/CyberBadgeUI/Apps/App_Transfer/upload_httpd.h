/**
 * @file upload_httpd.h
 * @brief 赛博吧唧 HTTP 上传服务器：手机浏览器选图上传到 SD 卡
 */
#ifndef _UPLOAD_HTTPD_H_
#define _UPLOAD_HTTPD_H_

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 启动 HTTP 服务器(监听 80 端口) */
esp_err_t upload_httpd_start(void);

/** 停止 HTTP 服务器 */
void upload_httpd_stop(void);

/** 返回本次会话累计成功上传的文件数(用于 UI 显示) */
uint32_t upload_httpd_get_count(void);

#ifdef __cplusplus
}
#endif

#endif
