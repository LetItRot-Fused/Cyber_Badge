/**
 * @file upload_httpd.c
 * @brief HTTP 上传服务器：内嵌网页 + 裁剪 360x360 + 接收文件写入 SD 卡
 *        裁剪交互：固定 360x360 方框，图片缩放+拖动来调整选取范围
 */
#include "upload_httpd.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include "esp_log.h"
#include "esp_http_server.h"

#define TAG "upload_httpd"

#define SD_MOUNT        "/sdcard"
#define RECV_BUF_SIZE   4096
#define MAX_NAME_LEN    96

static httpd_handle_t s_server = NULL;
static uint32_t s_upload_count = 0;

/* ============================================================
   内嵌上传页面
   裁剪交互设计：
   - 页面中央固定一个 360x360 的方框（可视窗口）
   - 图片在方框内显示，可以双指缩放和单指拖动
   - 方框外的图片被遮罩挡住
   - 确认后裁剪方框内的区域为 360x360 输出
   ============================================================ */
static const char UPLOAD_PAGE[] =
"<!DOCTYPE html><html lang=zh><head><meta charset=utf-8>"
"<meta name=viewport content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>"
"<title>赛博吧唧 传图</title><style>"
"*{box-sizing:border-box;-webkit-touch-callout:none;-webkit-user-select:none;user-select:none}"
"body{font-family:-apple-system,sans-serif;margin:0;padding:16px;background:#111;color:#eee}"
"h2{text-align:center}"
".card{background:#1d1d1f;border-radius:12px;padding:16px;margin:12px 0}"
"input[type=file]{width:100%;color:#eee}"
"button{width:100%;padding:14px;font-size:16px;border:0;border-radius:10px;background:#0a84ff;color:#fff;margin-top:12px}"
"button:disabled{background:#555}"
".bar{height:8px;background:#333;border-radius:4px;overflow:hidden;margin-top:8px}"
".bar>i{display:block;height:100%;width:0;background:#0a84ff;transition:width .2s}"
".item{display:flex;justify-content:space-between;align-items:center;padding:8px 0;border-bottom:1px solid #333}"
".item img{height:48px;width:48px;object-fit:cover;border-radius:6px}"
".del{background:#ff453a;width:auto;padding:6px 12px;margin:0;font-size:14px}"
"small{color:#888}"
".crop-overlay{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,.95);z-index:100;flex-direction:column;align-items:center;justify-content:center}"
".crop-overlay.active{display:flex}"
".crop-title{color:#fff;font-size:18px;margin-bottom:12px}"
".crop-frame{position:relative;width:360px;height:360px;border:2px solid #0a84ff;overflow:hidden;background:#000;touch-action:none}"
".crop-frame canvas{position:absolute;top:0;left:0;width:360px;height:360px}"
".crop-crosshair::before,.crop-crosshair::after{content:'';position:absolute;background:rgba(10,132,255,0.4);pointer-events:none;z-index:2}"
".crop-crosshair::before{top:50%;left:0;right:0;height:1px}"
".crop-crosshair::after{left:50%;top:0;bottom:0;width:1px}"
".crop-btns{display:flex;gap:12px;margin-top:16px;width:100%;max-width:320px;padding:0 16px}"
".crop-btns button{flex:1;margin:0;padding:12px;font-size:15px}"
".crop-btns .cancel-btn{background:#555}"
".crop-hint{color:#aaa;font-size:13px;margin-top:8px;text-align:center;max-width:300px}"
"</style></head><body>"
"<h2>赛博吧唧 手机传图</h2>"
"<div class=card>"
"<input id=f type=file accept='.jpg,.jpeg,.gif' multiple>"
"<button id=up onclick=startCrop()>选择并裁剪上传</button>"
"<div class=bar><i id=pb></i></div>"
"<small id=msg>支持 jpg / gif，可多选，自动裁剪为 360x360</small>"
"</div>"
"<div class=card><b>SD卡现有图片</b><div id=list>加载中...</div></div>"
"<div class=crop-overlay id=cropOverlay>"
"<div class=crop-title>拖动和缩放图片来调整裁剪区域</div>"
"<div class='crop-frame crop-crosshair' id=cropFrame><canvas id=cropCanvas></canvas></div>"
"<div class=crop-hint id=cropHint>单指拖动移动图片 / 双指缩放调整大小</div>"
"<div class=crop-btns>"
"<button class=cancel-btn onclick=cancelCrop()>取消</button>"
"<button onclick=confirmCrop()>确认裁剪</button>"
"</div></div>"
"<script>"
"var cropFiles=[],cropIdx=0,cropQueue=[];"
"var imgW=0,imgH=0;"
"var FRAME_SIZE=360;"
"var TARGET=360;"
"var scale=1,offX=0,offY=0;"
"var dragging=false,lastX=0,lastY=0;"
"var pinching=false,pinchDist=0,pinchScale=1,pinchCX=0,pinchCY=0,pinchOX=0,pinchOY=0;"
"var canvas,ctx;"
"function esc(s){return s.replace(/[&<>\"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;'}[c]))}"
"async function load(){try{let r=await fetch('/list');let a=await r.json();let h=a.length?'':'<small>暂无图片</small>';a.forEach(n=>{h+=`<div class=item><span><img src='/img?name=${encodeURIComponent(n)}'> ${esc(n)}</span><button class='del' onclick=\"del('${esc(n)}')\">删除</button></div>`});document.getElementById('list').innerHTML=h}catch(e){document.getElementById('list').innerHTML='<small>加载失败</small>'}}"
"async function del(n){if(!confirm('删除 '+n+' ?'))return;await fetch('/delete?name='+encodeURIComponent(n),{method:'DELETE'});load()}"
"function startCrop(){let fs=document.getElementById('f').files;if(!fs.length){alert('请先选择图片');return}cropFiles=Array.from(fs);cropIdx=0;cropQueue=[];showCropImage(cropFiles[0])}"
"function showCropImage(file){let url=URL.createObjectURL(file);let img=new Image();img.onload=function(){URL.revokeObjectURL(url);imgW=img.naturalWidth;imgH=img.naturalHeight;initCrop(img);};img.onerror=function(){URL.revokeObjectURL(url);alert('图片加载失败');cancelCrop();};img.src=url;document.getElementById('cropOverlay').classList.add('active');}"
"function initCrop(img){canvas=document.getElementById('cropCanvas');ctx=canvas.getContext('2d');canvas._img=img;canvas.width=FRAME_SIZE;canvas.height=FRAME_SIZE;let fitScale=Math.min(FRAME_SIZE/imgW,FRAME_SIZE/imgH);scale=fitScale;offX=(FRAME_SIZE-imgW*scale)/2;offY=(FRAME_SIZE-imgH*scale)/2;drawCrop();}"
"function drawCrop(){ctx.clearRect(0,0,FRAME_SIZE,FRAME_SIZE);ctx.save();ctx.translate(offX,offY);ctx.scale(scale,scale);ctx.drawImage(canvas._img,0,0,imgW,imgH);ctx.restore();}"
"function clampOffset(){let dw=imgW*scale,dh=imgH*scale;if(dw<=FRAME_SIZE){offX=(FRAME_SIZE-dw)/2}else{if(offX>0)offX=0;if(offX<FRAME_SIZE-dw)offX=FRAME_SIZE-dw}if(dh<=FRAME_SIZE){offY=(FRAME_SIZE-dh)/2}else{if(offY>0)offY=0;if(offY<FRAME_SIZE-dh)offY=FRAME_SIZE-dh}}"
"function cancelCrop(){document.getElementById('cropOverlay').classList.remove('active');dragging=false;pinching=false;}"
"function confirmCrop(){let tmpC=document.createElement('canvas');tmpC.width=TARGET;tmpC.height=TARGET;let tctx=tmpC.getContext('2d');let sx=-offX/scale;let sy=-offY/scale;let sSize=FRAME_SIZE/scale;tctx.drawImage(canvas._img,sx,sy,sSize,sSize,0,0,TARGET,TARGET);tmpC.toBlob(function(blob){var name='crop_'+Date.now()+'.jpg';cropQueue.push({blob:blob,name:name});document.getElementById('cropOverlay').classList.remove('active');dragging=false;pinching=false;cropIdx++;if(cropIdx<cropFiles.length){showCropImage(cropFiles[cropIdx]);}else{uploadCropped();}},'image/jpeg',0.92);}"
"async function uploadCropped(){if(!cropQueue.length){document.getElementById('msg').textContent='没有需要上传的图片';return}var btn=document.getElementById('up'),pb=document.getElementById('pb'),msg=document.getElementById('msg');btn.disabled=true;for(var i=0;i<cropQueue.length;i++){var item=cropQueue[i];msg.textContent='上传 '+item.name+' ('+(i+1)+'/'+cropQueue.length+')';try{await new Promise(function(res,rej){var x=new XMLHttpRequest();x.open('PUT','/upload?name='+encodeURIComponent(item.name));x.upload.onprogress=function(e){if(e.lengthComputable)pb.style.width=(e.loaded/e.total*100)+'%'};x.onload=function(){x.status==200?res():rej(x.responseText)};x.onerror=function(){rej('网络错误')};x.send(item.blob);})}catch(err){msg.textContent='失败: '+err;btn.disabled=false;return}}pb.style.width='100%';msg.textContent='全部上传完成 ('+cropQueue.length+'张)';btn.disabled=false;cropQueue=[];load();}"
/* 单指拖动 */
"document.getElementById('cropFrame').addEventListener('pointerdown',function(e){if(e.pointerType==='touch'&&!e.isPrimary)return;e.preventDefault();dragging=true;lastX=e.clientX;lastY=e.clientY;this.setPointerCapture(e.pointerId);});"
"document.getElementById('cropFrame').addEventListener('pointermove',function(e){if(!dragging)return;e.preventDefault();let dx=e.clientX-lastX,dy=e.clientY-lastY;offX+=dx;offY+=dy;lastX=e.clientX;lastY=e.clientY;clampOffset();drawCrop();});"
"document.getElementById('cropFrame').addEventListener('pointerup',function(){dragging=false;});"
"document.getElementById('cropFrame').addEventListener('pointercancel',function(){dragging=false;});"
/* 双指缩放 */
"document.getElementById('cropFrame').addEventListener('touchstart',function(e){if(e.touches.length===2){e.preventDefault();pinching=true;let t=e.touches;pinchDist=Math.hypot(t[0].clientX-t[1].clientX,t[0].clientY-t[1].clientY);pinchScale=scale;pinchCX=(t[0].clientX+t[1].clientX)/2;pinchCY=(t[0].clientY+t[1].clientY)/2;pinchOX=offX;pinchOY=offY;}},{passive:false});"
"document.getElementById('cropFrame').addEventListener('touchmove',function(e){if(!pinching||e.touches.length<2)return;e.preventDefault();let t=e.touches;let dist=Math.hypot(t[0].clientX-t[1].clientX,t[0].clientY-t[1].clientY);let newScale=pinchScale*(dist/pinchDist);let minS=Math.min(FRAME_SIZE/imgW,FRAME_SIZE/imgH);newScale=Math.max(minS,newScale);let cx=(t[0].clientX+t[1].clientX)/2;let cy=(t[0].clientY+t[1].clientY)/2;let rect=this.getBoundingClientRect();let rx=cx-rect.left-FRAME_SIZE/2;let ry=cy-rect.top-FRAME_SIZE/2;offX=pinchOX+rx*(1-newScale/pinchScale);offY=pinchOY+ry*(1-newScale/pinchScale);scale=newScale;clampOffset();drawCrop();},{passive:false});"
"document.getElementById('cropFrame').addEventListener('touchend',function(e){if(e.touches.length<2)pinching=false;});"
"document.getElementById('cropFrame').addEventListener('touchcancel',function(){pinching=false;});"
"load();"
"</script></body></html>";

/* ============================================================
   工具：URL 解码（就地，处理 %XX 和 +）
   ============================================================ */
static void url_decode(char *s)
{
    char *d = s;
    while (*s) {
        if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
            char h[3] = { s[1], s[2], 0 };
            *d++ = (char)strtol(h, NULL, 16);
            s += 3;
        } else if (*s == '+') {
            *d++ = ' ';
            s++;
        } else {
            *d++ = *s++;
        }
    }
    *d = 0;
}

/* 校验文件名：只允许 jpg/jpeg/gif，禁止路径穿越字符 */
static bool name_is_safe(const char *name)
{
    if (!name || !name[0] || strlen(name) >= MAX_NAME_LEN) return false;
    if (strstr(name, "..") || strchr(name, '/') || strchr(name, '\\')) return false;
    if (name[0] == '.') return false;

    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    char ext[8] = {0};
    size_t n = strlen(dot);
    if (n >= sizeof(ext)) return false;
    for (size_t i = 0; i < n; i++) ext[i] = (char)tolower((unsigned char)dot[i]);

    return (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0 ||
            strcmp(ext, ".gif") == 0);
}

/* 从 query string 取出 name 参数并解码，写入 out */
static bool get_name_param(httpd_req_t *req, char *out, size_t out_len)
{
    size_t qlen = httpd_req_get_url_query_len(req) + 1;
    if (qlen <= 1 || qlen > 256) return false;

    char query[256];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) return false;
    if (httpd_query_key_value(query, "name", out, out_len) != ESP_OK) return false;

    url_decode(out);
    return name_is_safe(out);
}

/* ============================================================
   路由处理
   ============================================================ */
static esp_err_t handle_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, UPLOAD_PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_upload(httpd_req_t *req)
{
    char name[MAX_NAME_LEN];
    if (!get_name_param(req, name, sizeof(name))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "非法文件名(仅支持 jpg/gif)");
        return ESP_FAIL;
    }

    char path[160];
    snprintf(path, sizeof(path), "%s/%s", SD_MOUNT, name);

    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "fopen失败: %s, errno=%d", path, errno);
        const char *err_msg = (errno == ENOSPC) ? "SD卡空间不足" :
                              (errno == EACCES) ? "SD卡写保护或无权限" :
                              (errno == EINVAL) ? "文件名无效(需开启FAT长文件名)" :
                              (errno == ENFILE || errno == EMFILE) ? "文件描述符耗尽" :
                              "SD卡写入失败";
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err_msg);
        return ESP_FAIL;
    }

    char *buf = malloc(RECV_BUF_SIZE);
    if (!buf) {
        fclose(f);
        remove(path);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "内存不足");
        return ESP_FAIL;
    }

    int remaining = req->content_len;
    bool ok = true;
    while (remaining > 0) {
        int recv = httpd_req_recv(req, buf, RECV_BUF_SIZE);
        if (recv <= 0) {
            if (recv == HTTPD_SOCK_ERR_TIMEOUT) continue;
            ok = false;
            break;
        }
        if (fwrite(buf, 1, recv, f) != (size_t)recv) {
            ok = false;
            break;
        }
        remaining -= recv;
    }

    free(buf);
    fclose(f);

    if (!ok) {
        remove(path);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "传输中断");
        return ESP_FAIL;
    }

    s_upload_count++;
    ESP_LOGI(TAG, "已保存: %s (%d bytes)", path, req->content_len);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t handle_list(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    DIR *dir = opendir(SD_MOUNT);
    if (!dir) {
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }

    httpd_resp_sendstr_chunk(req, "[");
    struct dirent *ent;
    bool first = true;
    while ((ent = readdir(dir)) != NULL) {
        if (!name_is_safe(ent->d_name)) continue;
        if (!first) httpd_resp_sendstr_chunk(req, ",");
        first = false;
        httpd_resp_sendstr_chunk(req, "\"");
        httpd_resp_sendstr_chunk(req, ent->d_name);
        httpd_resp_sendstr_chunk(req, "\"");
    }
    closedir(dir);
    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t handle_img(httpd_req_t *req)
{
    char name[MAX_NAME_LEN];
    if (!get_name_param(req, name, sizeof(name))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "非法文件名");
        return ESP_FAIL;
    }

    char path[160];
    snprintf(path, sizeof(path), "%s/%s", SD_MOUNT, name);
    FILE *f = fopen(path, "rb");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "文件不存在");
        return ESP_FAIL;
    }

    const char *dot = strrchr(name, '.');
    httpd_resp_set_type(req, (dot && strcasecmp(dot, ".gif") == 0) ? "image/gif" : "image/jpeg");

    char *buf = malloc(RECV_BUF_SIZE);
    if (!buf) { fclose(f); return ESP_FAIL; }
    size_t r;
    while ((r = fread(buf, 1, RECV_BUF_SIZE, f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, r) != ESP_OK) break;
    }
    free(buf);
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t handle_delete(httpd_req_t *req)
{
    char name[MAX_NAME_LEN];
    if (!get_name_param(req, name, sizeof(name))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "非法文件名");
        return ESP_FAIL;
    }

    char path[160];
    snprintf(path, sizeof(path), "%s/%s", SD_MOUNT, name);
    if (remove(path) != 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "删除失败");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "已删除: %s", path);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

/* ============================================================
   启停
   ============================================================ */
esp_err_t upload_httpd_start(void)
{
    if (s_server) return ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 8192;
    config.max_uri_handlers = 8;
    config.recv_wait_timeout = 15;
    config.send_wait_timeout = 15;
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd 启动失败: %s", esp_err_to_name(ret));
        s_server = NULL;
        return ret;
    }

    httpd_uri_t routes[] = {
        { .uri = "/",        .method = HTTP_GET,    .handler = handle_root },
        { .uri = "/upload",  .method = HTTP_PUT,    .handler = handle_upload },
        { .uri = "/upload",  .method = HTTP_POST,   .handler = handle_upload },
        { .uri = "/list",    .method = HTTP_GET,    .handler = handle_list },
        { .uri = "/img",     .method = HTTP_GET,    .handler = handle_img },
        { .uri = "/delete",  .method = HTTP_DELETE, .handler = handle_delete },
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(s_server, &routes[i]);
    }

    s_upload_count = 0;
    ESP_LOGI(TAG, "HTTP 上传服务器已启动, 访问 http://192.168.4.1");
    return ESP_OK;
}

void upload_httpd_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "HTTP 服务器已停止");
    }
}

uint32_t upload_httpd_get_count(void)
{
    return s_upload_count;
}
