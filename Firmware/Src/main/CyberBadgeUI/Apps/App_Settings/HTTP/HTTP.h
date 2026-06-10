#ifndef HTTP_H
#define HTTP_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 导出全局天气数据（给 C++ 调用）
extern char weather_city[];
extern char weather_temp[];
extern char weather_text[];

// 公开函数
esp_err_t weather_http_init(void);
esp_err_t location_http_init(void);
void obtain_time(void);

#ifdef __cplusplus
}
#endif

#endif