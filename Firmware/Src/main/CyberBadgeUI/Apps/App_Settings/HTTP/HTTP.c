#include <stdio.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_sntp.h"          // SNTP 时间同步（esp_sntp_*）
#include "esp_netif_sntp.h"    // esp_netif_sntp_deinit
#include <time.h>              // time, localtime_r, clock_settime
#include <sys/time.h>          // struct timeval
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define TAG "HTTP"

#define WIFI_CONNECT_BIT    BIT0
static EventGroupHandle_t s_wifi_ev = NULL;

//心知天气秘钥
#define WEATHER_PRIVATE_KEY "SX3i-wrEj5RcmFPQf"



#define MAX_OUTPUT_BUFFER_LEN   1024
static  char output_buffer[MAX_OUTPUT_BUFFER_LEN] = {0};   //用于接收通过http协议返回的数据
static int output_len = 0;

//当获取到城市的名字后，保存在如下数组
// 心知天气 全局数据
char  weather_city[64] = {0};
char  weather_temp[16] = {0};
char  weather_text[32] = {0};

bool sntp_initialized = false;

#define CLIENT_HTTP_RECEIVE_BIT BIT0
static EventGroupHandle_t   s_client_http_event = NULL; //此事件用于通知http接收完成




static esp_err_t http_client_event_handler(esp_http_client_event_t *evt)
{
    if (s_client_http_event == NULL) return ESP_OK;
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:    //错误事件
            //ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:    //连接成功事件
            //ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:    //发送头事件
            //ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:    //接收头事件
            //ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
            printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:    //接收数据事件
            //ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            xEventGroupClearBits(s_client_http_event,CLIENT_HTTP_RECEIVE_BIT);
            if(output_len + evt->data_len < MAX_OUTPUT_BUFFER_LEN)
            {
                memcpy(&output_buffer[output_len], evt->data,evt->data_len);
                output_len += evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:    //会话完成事件
            //ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            xEventGroupSetBits(s_client_http_event,CLIENT_HTTP_RECEIVE_BIT);
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:    //断开事件
            //ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            //ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

/** 解析返回的天气数据
 * @param json数据
 * @return ESP_OK or ESP_FAIL
*/
static esp_err_t parse_weather(char* weather_js)
{
    cJSON *wt_js = cJSON_Parse(weather_js);
    if(!wt_js)
    {
        ESP_LOGI(TAG,"invaild json format");
        return ESP_FAIL;
    }
    cJSON *result_js = cJSON_GetObjectItem(wt_js,"results");
    if(!result_js)
    {
        ESP_LOGI(TAG,"invaild weather result");
        return ESP_FAIL;
    }
    cJSON *result_child_js = result_js->child;
    while(result_child_js)
    {
        cJSON *location_js = cJSON_GetObjectItem(result_child_js,"location");
        cJSON *name_js = NULL;
        if(location_js)
        {
            name_js = cJSON_GetObjectItem(location_js,"name");
        }
        cJSON *now_js = cJSON_GetObjectItem(result_child_js,"now");
        cJSON *temp_js = NULL;
        cJSON *text_js = NULL;
        if(now_js)
        {
            temp_js = cJSON_GetObjectItem(now_js,"temperature");
            text_js = cJSON_GetObjectItem(now_js,"text");
        }
        if(name_js && temp_js)
{
    // 保存到全局变量
    snprintf(weather_city, sizeof(weather_city), "%s", cJSON_GetStringValue(name_js));
    snprintf(weather_temp, sizeof(weather_temp), "%s", cJSON_GetStringValue(temp_js));
    snprintf(weather_text, sizeof(weather_text), "%s", cJSON_GetStringValue(text_js));

    ESP_LOGI(TAG,"city:%s,temperature:%s,weather:%s",
             weather_city, weather_temp, weather_text);
}
        result_child_js = result_child_js->next;
    }
    return ESP_OK;
}

/** 解析返回的位置数据
 * @param json数据
 * @return ESP_OK or ESP_FAIL
*/
static esp_err_t parse_location(char* location_js)
{
    cJSON *lo_js = cJSON_Parse(location_js);
    if(!lo_js)
    {
        ESP_LOGI(TAG,"invaild json format");
        return ESP_FAIL;
    }
    cJSON *city_js = cJSON_GetObjectItem(lo_js,"city");
    if(!city_js)
    {
        ESP_LOGI(TAG,"invaild location result");
        return ESP_FAIL;
    }
    snprintf(weather_city,sizeof(weather_city),"%s",cJSON_GetStringValue(city_js));
    return ESP_OK;
}

/** 发起http请求，获取天气数据
 * @param 无
 * @return ESP_OK or ESP_FAIL
*/
esp_err_t weather_http_init(void)
{

    if (s_client_http_event == NULL)
        s_client_http_event = xEventGroupCreate();
    esp_err_t ret_code = ESP_FAIL;
    static char weather_url[256];

    // 定义http配置结构体
    esp_http_client_config_t config;
    snprintf(weather_url,sizeof(weather_url),
    "http://api.seniverse.com/v3/weather/now.json?key=%s&location=%s&language=zh-Hans&unit=c",
        WEATHER_PRIVATE_KEY,weather_city);
    
    config.url = weather_url;
    config.event_handler = http_client_event_handler;
    config.disable_auto_redirect = true; // 必须加！禁止跳HTTPS
    
    // 初始化结构体
    esp_http_client_handle_t http_client = esp_http_client_init(&config);
    if(!http_client)
    {
        ESP_LOGI(TAG,"http_client init fail!");
        return ESP_FAIL;
    }

    // 【重要】心知天气必须用 GET，不是 POST！
    esp_http_client_set_method(http_client, HTTP_METHOD_GET);

    esp_err_t err  = esp_http_client_perform(http_client);
    if(err != ESP_OK)
    {
        return ESP_FAIL;
    }
    //获取返回的内容长度
    output_len =  esp_http_client_get_content_length(http_client);
    if(output_len > 0)
    {
        output_len = esp_http_client_read(http_client, output_buffer, MAX_OUTPUT_BUFFER_LEN);
        ret_code = parse_weather(output_buffer);
        output_len = 0;
    }
    else
    {
        //chunked块，需要从https event_handle获取数据
        EventBits_t ev = xEventGroupWaitBits(s_client_http_event,CLIENT_HTTP_RECEIVE_BIT,pdTRUE,pdFALSE,10*1000/portTICK_PERIOD_MS);
        if(ev & CLIENT_HTTP_RECEIVE_BIT)
        {
            ret_code = parse_weather(output_buffer);
            output_len = 0;
        }
    }
    return ret_code;
}

/** 发起http请求，获取当前城市
 * @param 无
 * @return ESP_OK or ESP_FAIL
*/
esp_err_t location_http_init(void)
{

    if (s_client_http_event == NULL)
        s_client_http_event = xEventGroupCreate();
    esp_err_t ret_code = ESP_OK;

    // 定义http配置结构体
    esp_http_client_config_t config = {
        .url = "http://ip-api.com/json/?lang=en",
        .event_handler = http_client_event_handler,
        .disable_auto_redirect = true, // 禁止跳转
    };
    
    // 初始化结构体
    esp_http_client_handle_t http_client = esp_http_client_init(&config);
    if(!http_client)
    {
        ESP_LOGI(TAG,"http_client init fail!");
        return ESP_FAIL;
    }

    // 【重要】GET 不是 POST！
    esp_http_client_set_method(http_client, HTTP_METHOD_GET);

    esp_err_t err  = esp_http_client_perform(http_client);
    if(err != ESP_OK)
    {
        return ESP_FAIL;
    }
    output_len =  esp_http_client_get_content_length(http_client);
    if(output_len > 0)
    {
        output_len = esp_http_client_read(http_client, output_buffer, MAX_OUTPUT_BUFFER_LEN);
        ret_code = parse_location(output_buffer);
        output_len = 0;
    }
    else
    {
        //chunked块，需要从https event_handle获取数据
        EventBits_t ev = xEventGroupWaitBits(s_client_http_event,CLIENT_HTTP_RECEIVE_BIT,pdTRUE,pdFALSE,10*1000/portTICK_PERIOD_MS);
        if(ev & CLIENT_HTTP_RECEIVE_BIT)
        {
            ret_code = parse_location(output_buffer);
            output_len = 0;
        }
    }
    return ret_code;
}
void sntp_sync_time_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "🔄 SNTP 自动校准开始");

    // 官方默认时间更新逻辑（无编译错误）
    struct timespec ts;
    ts.tv_sec = tv->tv_sec;
    ts.tv_nsec = tv->tv_usec * 1000;
    clock_settime(CLOCK_REALTIME, &ts);
    // 打印时间
    time_t now = tv->tv_sec;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    ESP_LOGW(TAG, "校准后时间: %04d-%02d-%02d %02d:%02d:%02d",
             1900 + timeinfo.tm_year,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
}
void obtain_time(void)
{
    if (sntp_initialized) {
        ESP_LOGI(TAG, "SNTP 已经初始化过，跳过重新初始化");
        return;
    }else{
        esp_netif_sntp_deinit();
        // 初始化 NTP
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "ntp1.aliyun.com");
        esp_sntp_set_time_sync_notification_cb(sntp_sync_time_cb);
        sntp_set_sync_interval(60*60*1000);//60分钟
        esp_sntp_init();
        sntp_initialized = true;

        int retry = 0;
        bool ntp_success = false;

        // 等待 NTP 同步（只判断同步状态，不判断时间）
        while (retry < 10)
        {
            if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED)
            {
                ntp_success = true;
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
            retry++;
            ESP_LOGI(TAG, "等待NTP同步...%d/10", retry);
        }


        // 获取时间（NTP成功则是网络时间，失败则自动读取RTC时间）
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        // 输出结果
        if (ntp_success)
        {
            ESP_LOGW(TAG, "NTP同步成功");
        }
        else
        {
            ESP_LOGW(TAG, "NTP同步失败，使用RTC本地时间");
        }

        // 打印最终时间
        ESP_LOGW(TAG, "当前时间: %04d-%02d-%02d %02d:%02d:%02d",
                 1900 + timeinfo.tm_year,
                 timeinfo.tm_mon + 1,
                 timeinfo.tm_mday,
                 timeinfo.tm_hour,
                 timeinfo.tm_min,
                 timeinfo.tm_sec);
    }
}