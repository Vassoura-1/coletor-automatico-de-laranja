// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <SPIFFS.h>

#include <esp_http_server.h>
#include <esp_timer.h>
#include <esp_camera.h>
#include <img_converters.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "fb_gfx.h"
#include "fd_forward.h"
#include "fr_forward.h"

#include "fallback_index.h"
#include "utilities.h"
#include "machine_vision.h"
#include "machine_vision_loop.h"
#include "labelling.h"

const size_t circle_buf_h_len = 40*4;
uint8_t circle_array_h[circle_buf_h_len] = {};
uint8_t * circle_buf_h = circle_array_h;

uint8_t threshold = 108;

// estrutura com dados de cabeçalho para streaming jpg
typedef struct {
        httpd_req_t *req;
        size_t len;
} jpg_chunking_t;

// Define alguns cabeçalhos para respostas http
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
// static const char* _VISION_PART = "Content-Type: image/bmp\r\nContent-Length: %u\r\n\r\n";

//filtro de média móvel
static ra_filter_t ra_filter;

//ponteiros armazenando os servidores
httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

// função para escrever em cima do buffer de imagem
// static void rgb_print(dl_matrix3du_t *image_matrix, uint32_t color, const char * str){
//     fb_data_t fb;
//     fb.width = image_matrix->w;
//     fb.height = image_matrix->h;
//     fb.data = image_matrix->item;
//     fb.bytes_per_pixel = 3;
//     fb.format = FB_BGR888;
//     fb_gfx_print(&fb, (fb.width - (strlen(str) * 14)) / 2, 10, color, str);
// }

// função para escrever em cima do buffer de imagem
// static int rgb_printf(dl_matrix3du_t *image_matrix, uint32_t color, const char *format, ...){
//     char loc_buf[64];
//     char * temp = loc_buf;
//     int len;
//     va_list arg;
//     va_list copy;
//     va_start(arg, format);
//     va_copy(copy, arg);
//     len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
//     va_end(copy);
//     if(len >= sizeof(loc_buf)){
//         temp = (char*)malloc(len+1);
//         if(temp == NULL) {
//             return 0;
//         }
//     }
//     vsnprintf(temp, len+1, format, arg);
//     va_end(arg);
//     rgb_print(image_matrix, color, temp);
//     if(len > 64){
//         free(temp);
//     }
//     return len;
// }

// envia um pedaço do jpg na resposta http
static size_t jpg_encode_stream(void * arg, size_t index, const void* data, size_t len){
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if(!index){
        j->len = 0;
    }
    if(httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK){
        return 0;
    }
    j->len += len;
    return len;
}

// processa a requisição de uma foto
static esp_err_t capture_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        log_e("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    size_t fb_len = 0;
    if(fb->format == PIXFORMAT_JPEG){
        fb_len = fb->len;
        res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    } else {
        jpg_chunking_t jchunk = {req, 0};
        res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk)?ESP_OK:ESP_FAIL;
        httpd_resp_send_chunk(req, NULL, 0);
        fb_len = jchunk.len;
    }
    esp_camera_fb_return(fb);
    int64_t fr_end = esp_timer_get_time();
    log_i("JPG: %uB %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start)/1000));

    return res;
}

// processa a requisição de uma foto com filtros cv
esp_err_t vision_capture_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        log_e("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    const size_t height = fb->height;
    const size_t width = fb->width;

    const size_t vision_buf_len = height*width;
    uint8_t * vision_buf = (uint8_t *) heap_caps_malloc(vision_buf_len,MALLOC_CAP_SPIRAM);
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    log_i("Free heap: %d", ESP.getFreeHeap());
    log_i("Free PSRAM: %d", ESP.getFreePsram());

    if(!color_filtering(fb, vision_buf, vision_buf_len, threshold)){
        log_e("Error applying color filtering");
        esp_camera_fb_return(fb);
        heap_caps_free(vision_buf);
        vision_buf = NULL;
        return ESP_FAIL;
    }

    esp_camera_fb_return(fb);
    
    log_i("Free heap: %d", ESP.getFreeHeap());
    log_i("Free PSRAM: %d", ESP.getFreePsram());

    // // apply edge detection
    // if(!edge_detection(width, height, vision_buf, vision_buf_len)){
    //     log_e("Error applying edge detection");
    //     return;
    // }

    // // apply shape recognition
    // if(!shape_recognition(width, height, vision_buf, vision_buf_len, max_label, circle_buf_h, circle_buf_h_len)){
    //     log_e("Error applying shape recognition");
    //     return;
    // }

    size_t fb_len = 0;
    if(!fmt2jpg(vision_buf,vision_buf_len,width,height,PIXFORMAT_GRAYSCALE,80,&_jpg_buf,&_jpg_buf_len)){
        log_e("Convert to JPEG failed");
        heap_caps_free(vision_buf);
        vision_buf = NULL;
        return ESP_FAIL;
    }
    fb_len = _jpg_buf_len;
    res = httpd_resp_send(req, (const char *)_jpg_buf, _jpg_buf_len);

    heap_caps_free(vision_buf);
    vision_buf = NULL;
    
    int64_t fr_end = esp_timer_get_time();
    log_i("JPG: %uB %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start)/1000));

    return res;
}

// processa a requisição de uma foto com filtros cv
esp_err_t vision_capture_handler2(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        log_e("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    const size_t height = fb->height;
    const size_t width = fb->width;

    const size_t vision_buf_len = height*width;
    uint8_t * vision_buf = (uint8_t *) heap_caps_malloc(vision_buf_len,MALLOC_CAP_SPIRAM);
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    log_i("Free heap: %d", ESP.getFreeHeap());
    log_i("Free PSRAM: %d", ESP.getFreePsram());

    if(!color_filtering(fb, vision_buf, vision_buf_len, threshold)){
        log_e("Error applying color filtering");
        esp_camera_fb_return(fb);
        heap_caps_free(vision_buf);
        vision_buf = NULL;
        return ESP_FAIL;
    }

    esp_camera_fb_return(fb);

    log_i("Free heap: %d", ESP.getFreeHeap());
    log_i("Free PSRAM: %d", ESP.getFreePsram());


    // // apply size filtering
    uint8_t * vision_buf2 = (uint8_t *) heap_caps_calloc(vision_buf_len, 1, MALLOC_CAP_SPIRAM);
    uint8_t max_label = LabelImage(width,height,vision_buf,vision_buf2,3000,200);
    if(!max_label || max_label == 255){
        log_e("Error applying size filtering"); 
        log_e("max_label = %d", max_label);
        return ESP_FAIL;
    }

    heap_caps_free(vision_buf);
    vision_buf = vision_buf2;

    log_e("max_label = %d", max_label);
    
    log_i("Free heap: %d", ESP.getFreeHeap());
    log_i("Free PSRAM: %d", ESP.getFreePsram());

    // // apply edge detection
    // if(!edge_detection(width, height, vision_buf, vision_buf_len)){
    //     log_e("Error applying edge detection");
    //     return ESP_FAIL;
    // }

    // // apply shape recognition
    // if(!shape_recognition(width, height, vision_buf, vision_buf_len, max_label, circle_buf_h, circle_buf_h_len)){
    //     log_e("Error applying shape recognition");
    //     return;
    // }

    size_t fb_len = 0;
    if(!fmt2jpg(vision_buf,vision_buf_len,width,height,PIXFORMAT_GRAYSCALE,80,&_jpg_buf,&_jpg_buf_len)){
        log_e("Convert to JPEG failed");
        heap_caps_free(vision_buf);
        vision_buf = NULL;
        return ESP_FAIL;
    }
    fb_len = _jpg_buf_len;
    res = httpd_resp_send(req, (const char *)_jpg_buf, _jpg_buf_len);

    heap_caps_free(vision_buf);
    vision_buf = NULL;
    
    int64_t fr_end = esp_timer_get_time();
    log_i("JPG: %uB %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start)/1000));

    return res;
}

// processa a requisição de uma foto com filtros cv
esp_err_t vision_capture_handler3(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        log_e("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int64_t fr_end = esp_timer_get_time();
    log_i("Got image in: %ums\n", (uint32_t)((fr_end - fr_start)/1000));

    const size_t height = fb->height;
    const size_t width = fb->width;

    log_i("width %u, height %u", width, height);

    const size_t vision_buf_len = height*width;
    uint8_t * vision_buf = (uint8_t *) heap_caps_malloc(vision_buf_len,MALLOC_CAP_SPIRAM);
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if(!color_filtering(fb, vision_buf, vision_buf_len, threshold)){
        log_e("Error applying color filtering");
        esp_camera_fb_return(fb);
        heap_caps_free(vision_buf);
        vision_buf = NULL;
        return ESP_FAIL;
    }

    esp_camera_fb_return(fb);

    
    fr_end = esp_timer_get_time();
    log_i("Color filtering in: %ums\n", (uint32_t)((fr_end - fr_start)/1000));

    // // apply size filtering
    // uint8_t * vision_buf2 = (uint8_t *) heap_caps_calloc(vision_buf_len, 1, MALLOC_CAP_SPIRAM);
    // uint8_t max_label = LabelImage(width,height,vision_buf,vision_buf,3000,100);
    // if(!max_label || max_label == 255){
    //     log_e("Error applying size filtering"); 
    //     log_e("max_label = %d", max_label);
    //     return ESP_FAIL;
    // }

    // for(int i = 0; i<vision_buf_len; i++){
    //     vision_buf[i] = (vision_buf[i]>0)?2:0;
    // }

    // fr_end = esp_timer_get_time();
    // log_i("Size filtering in: %ums\n", (uint32_t)((fr_end - fr_start)/1000));

    // log_i("Free heap: %d", ESP.getFreeHeap());
    // log_i("Free PSRAM: %d", ESP.getFreePsram());

    // // apply edge detection
    if(!edge_detection(width, height, vision_buf, vision_buf_len)){
        log_e("Error applying edge detection");
        heap_caps_free(vision_buf);
        return ESP_FAIL;
    }

    fr_end = esp_timer_get_time();
    log_i("Edge detection in: %ums\n", (uint32_t)((fr_end - fr_start)/1000));

    // apply shape recognition
    if(!shape_recognition2(width, height, vision_buf, vision_buf_len, circle_buf_h, circle_buf_h_len)){
        log_e("Error applying shape recognition");
        heap_caps_free(vision_buf);
        return ESP_FAIL;
    }
    
    fr_end = esp_timer_get_time();
    log_i("Shape recognition in: %ums\n", (uint32_t)((fr_end - fr_start)/1000));

    for (int i = 0; i < (circle_buf_h_len/4); i++)
    {
        if (circle_buf_h[4*i+3])
        {
            log_i("C.G %d: x %u, y %u, r %u, n %u", i+1,circle_buf_h[4*i],circle_buf_h[4*i+1],circle_buf_h[4*i+2],circle_buf_h[4*i+3]);
        }
    }

    uint8_t scale = 255 / 3;

    for(int i = 0; i<vision_buf_len; i++){
        vision_buf[i] = vision_buf[i]*scale;
    }

    size_t fb_len = 0;
    if(!fmt2jpg(vision_buf,vision_buf_len,width,height,PIXFORMAT_GRAYSCALE,80,&_jpg_buf,&_jpg_buf_len)){
        log_e("Convert to JPEG failed");
        heap_caps_free(vision_buf);
        return ESP_FAIL;
    }
    fb_len = _jpg_buf_len;
    res = httpd_resp_send(req, (const char *)_jpg_buf, _jpg_buf_len);

    heap_caps_free(vision_buf);
    vision_buf = NULL;
    
    
    fr_end = esp_timer_get_time();
    log_i("JPG: %uB %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start)/1000));

    return res;
}


// processa a requisição de streaming jgp
static esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];

    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    //Stream loop
    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            log_e("Camera capture failed");
            res = ESP_FAIL;
        } else {
            // Handle jpeg buffer conversion if necessary
            if(fb->format != PIXFORMAT_JPEG){
                bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                esp_camera_fb_return(fb);
                fb = NULL;
                if(!jpeg_converted){
                    log_e("JPEG compression failed");
                    res = ESP_FAIL;
                }
            } else {
                _jpg_buf_len = fb->len;
                _jpg_buf = fb->buf;
            }
        }

        //Send stream chunk
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }

        //Release framebuffer
        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }
        if(_jpg_buf){
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if(res != ESP_OK){
            break;
        }
        int64_t fr_end = esp_timer_get_time();
        
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);
        log_i("MJPG: %uB %ums (%.1ffps), AVG: %ums (%.1ffps)\n\r",
            (uint32_t)(_jpg_buf_len),
            (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time,
            avg_frame_time, 1000.0 / avg_frame_time
        );
        yield();
    }

    last_frame = 0;
    return res;
}

// Stream handler com filtro para cv
static esp_err_t vision_stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];

    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    //Stream loop
    // TODO: this leads to problems when trying to stream more than one video
    //       need to find a better solution
    while(true){
        fb = esp_camera_fb_get();

        // Computer vision processing function, always returns jpeg buffer
        if (!process_image_buffer(fb, &_jpg_buf, &_jpg_buf_len, threshold)){
            log_e("Error applying computer vision filter");
            break;
        }

        //Send stream chunk
        log_v("starting frame transmission");
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
            log_v("header sent");
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
            log_v("frame sent");
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
            log_v("boundary sent");
        }

        //Release framebuffer
        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
        }
        if(_jpg_buf){
        //Release jpg buffer
            free(_jpg_buf);
            _jpg_buf = NULL;
        } 

        if(res != ESP_OK){
            break;
        }

        // log average frames per second
        int64_t fr_end = esp_timer_get_time();        
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);
        log_i("MJPG: %uB %ums (%.1ffps), AVG: %ums (%.1ffps)\n\r",
            (uint32_t)(_jpg_buf_len),
            (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time,
            avg_frame_time, 1000.0 / avg_frame_time
        );
        yield();
    }

    last_frame = 0;
    return res;
}

// procressa requisições para a mudança de parâmetros da câmera
static esp_err_t cmd_handler(httpd_req_t *req){
    char*  buf;
    size_t buf_len;
    char variable[32] = {0,};
    char value[32] = {0,};

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if(!buf){
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
                httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) 
            {
            } else {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        free(buf);
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    int val = atoi(value);
    sensor_t * s = esp_camera_sensor_get();
    int res = 0;

    if(!strcmp(variable, "framesize")) res = s->set_framesize(s, (framesize_t)val);
    else if(!strcmp(variable, "treshould_value")) threshold = (uint8_t)val;
    else if(!strcmp(variable, "quality")) res = s->set_quality(s, val);
    else if(!strcmp(variable, "contrast")) res = s->set_contrast(s, val);
    else if(!strcmp(variable, "brightness")) res = s->set_brightness(s, val);
    else if(!strcmp(variable, "saturation")) res = s->set_saturation(s, val);
    else if(!strcmp(variable, "gainceiling")) res = s->set_gainceiling(s, (gainceiling_t)val);
    else if(!strcmp(variable, "colorbar")) res = s->set_colorbar(s, val);
    else if(!strcmp(variable, "awb")) res = s->set_whitebal(s, val);
    else if(!strcmp(variable, "agc")) res = s->set_gain_ctrl(s, val);
    else if(!strcmp(variable, "aec")) res = s->set_exposure_ctrl(s, val);
    else if(!strcmp(variable, "hmirror")) res = s->set_hmirror(s, val);
    else if(!strcmp(variable, "vflip")) res = s->set_vflip(s, val);
    else if(!strcmp(variable, "awb_gain")) res = s->set_awb_gain(s, val);
    else if(!strcmp(variable, "agc_gain")) res = s->set_agc_gain(s, val);
    else if(!strcmp(variable, "aec_value")) res = s->set_aec_value(s, val);
    else if(!strcmp(variable, "aec2")) res = s->set_aec2(s, val);
    else if(!strcmp(variable, "dcw")) res = s->set_dcw(s, val);
    else if(!strcmp(variable, "bpc")) res = s->set_bpc(s, val);
    else if(!strcmp(variable, "wpc")) res = s->set_wpc(s, val);
    else if(!strcmp(variable, "raw_gma")) res = s->set_raw_gma(s, val);
    else if(!strcmp(variable, "lenc")) res = s->set_lenc(s, val);
    else if(!strcmp(variable, "special_effect")) res = s->set_special_effect(s, val);
    else if(!strcmp(variable, "wb_mode")) res = s->set_wb_mode(s, val);
    else if(!strcmp(variable, "ae_level")) res = s->set_ae_level(s, val);
    else {
        res = -1;
    }

    if(res){
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

// processa a requisição solicitando os status da aplicação
static esp_err_t status_handler(httpd_req_t *req){
    static char json_response[1024];

    sensor_t * s = esp_camera_sensor_get();
    char * p = json_response;
    *p++ = '{';

    p+=sprintf(p, "\"framesize\":%u,", s->status.framesize);
    p+=sprintf(p, "\"threshold\":%u,", threshold);
    p+=sprintf(p, "\"quality\":%u,", s->status.quality);
    p+=sprintf(p, "\"brightness\":%d,", s->status.brightness);
    p+=sprintf(p, "\"contrast\":%d,", s->status.contrast);
    p+=sprintf(p, "\"saturation\":%d,", s->status.saturation);
    p+=sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
    p+=sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
    p+=sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
    p+=sprintf(p, "\"awb\":%u,", s->status.awb);
    p+=sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
    p+=sprintf(p, "\"aec\":%u,", s->status.aec);
    p+=sprintf(p, "\"aec2\":%u,", s->status.aec2);
    p+=sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
    p+=sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
    p+=sprintf(p, "\"agc\":%u,", s->status.agc);
    p+=sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
    p+=sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
    p+=sprintf(p, "\"bpc\":%u,", s->status.bpc);
    p+=sprintf(p, "\"wpc\":%u,", s->status.wpc);
    p+=sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
    p+=sprintf(p, "\"lenc\":%u,", s->status.lenc);
    p+=sprintf(p, "\"vflip\":%u,", s->status.vflip);
    p+=sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
    p+=sprintf(p, "\"dcw\":%u,", s->status.dcw);
    p+=sprintf(p, "\"colorbar\":%u,", s->status.colorbar);
    p+=sprintf(p, "\"free_Heap\":%d,", ESP.getFreeHeap());
    p+=sprintf(p, "\"free_PSRAM\":%d", ESP.getFreePsram());
    *p++ = '}';
    *p++ = 0;

    log_v("%s",json_response);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

// Serve a página inicial da aplicação usando SPIFFS
static esp_err_t index_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    File index_file = SPIFFS.open("/index.html.gz");
    size_t file_size = index_file.size();

    if(file_size > 0){
        char * buffer = (char *) malloc(file_size);

        while(index_file.available()){
            index_file.readBytes(buffer, file_size);
        }

        esp_err_t flag = httpd_resp_send(req, buffer, file_size);

        index_file.close();
        free(buffer);
        
        return flag;
    } else {
        index_file.close();
        return httpd_resp_send(req, (const char *)fallback_html_gz, fallback_html_gz_len);
    }
}

// Inicializa os servidores HTTP e associa cada handler a sua URL
void startCameraServer(){
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t cmd_uri = {
        .uri       = "/control",
        .method    = HTTP_GET,
        .handler   = cmd_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t capture_uri = {
        .uri       = "/capture",
        .method    = HTTP_GET,
        .handler   = capture_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t capture_uri2 = {
        .uri       = "/capture2",
        .method    = HTTP_GET,
        .handler   = vision_capture_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t capture_uri3 = {
        .uri       = "/capture3",
        .method    = HTTP_GET,
        .handler   = vision_capture_handler2,
        .user_ctx  = NULL
    };

    httpd_uri_t capture_uri4 = {
        .uri       = "/capture4",
        .method    = HTTP_GET,
        .handler   = vision_capture_handler3,
        .user_ctx  = NULL
    };

    httpd_uri_t stream_uri1 = {
        .uri       = "/stream1",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t stream_uri2 = {
        .uri       = "/stream2",
        .method    = HTTP_GET,
        .handler   = vision_stream_handler,
        .user_ctx  = NULL
    };

    ra_filter_init(&ra_filter, 20);
    
    log_i("Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &cmd_uri);
        httpd_register_uri_handler(camera_httpd, &status_uri);
        httpd_register_uri_handler(camera_httpd, &capture_uri);
        httpd_register_uri_handler(camera_httpd, &capture_uri2);
        httpd_register_uri_handler(camera_httpd, &capture_uri3);
        httpd_register_uri_handler(camera_httpd, &capture_uri4);
    } else {
        log_e("Failed to start web server");
    }

    config.server_port += 1;
    config.ctrl_port += 1;
    log_i("Starting stream server on port: '%d'", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri1);
        httpd_register_uri_handler(stream_httpd, &stream_uri2);
    } else {
        log_e("Failed to start stream server");
    }
}

// Encerra os servidores
void stopCameraServer(){
    if(camera_httpd != NULL){
        if(httpd_stop(camera_httpd) == ESP_OK)
            log_i("Servidor web encerrado");
    }
    if(stream_httpd != NULL){
        if(httpd_stop(stream_httpd) == ESP_OK)
            log_i("Servidor de streaming encerrado");
    }
}