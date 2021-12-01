#include "Arduino.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"

#include "machine_vision.h"

bool process_image_buffer(camera_fb_t *fb, uint8_t **_jpg_buf, size_t *_jpg_buf_len, uint8_t threshold){
    size_t rgb_buf_len = fb->height*fb->width*3;
    uint8_t * rgb_buf = (uint8_t *) heap_caps_malloc(rgb_buf_len*3,MALLOC_CAP_SPIRAM);
    int offset = rgb_buf_len;
    bool success = true;
    
    // get rgb buffer
    if(fb->format != PIXFORMAT_RGB888){
        if(!fmt2rgb888(fb->buf, fb->len, fb->format, rgb_buf)){
            log_e("Convert to RGB888 failed");
            success = false;
        }
    } else {
        //for images on top of each other
        memcpy(rgb_buf,fb->buf,fb->len);

        //for images side by side
        // uint8_t * tmp1 = fb->buf;
        // uint8_t * tmp2 = rgb_buf;
        // for(int i = 0; i < (fb->width*fb->height); i+=1)
        // {
        //     * tmp2++ = * tmp1++;
        //     * tmp2++ = * tmp1++;
        //     * tmp2++ = * tmp1++;
        //     if(i!=0 && i%fb->width){
        //         tmp2 = tmp2 + 2*offset;
        //     }
        // }
    }

    float tmp = 0;
    uint8_t * position = rgb_buf;
    uint8_t * position2 = rgb_buf + offset;
    uint8_t * position3 = rgb_buf + 2*offset;
    // image data bytes are stored as a trail of BGR bytes read from left to right, top to bottom
    for (int i = 0; i < (fb->width*fb->height); i+=1){
        //access pixel color bytes
        uint8_t b = *position++;
        uint8_t g = *position++;
        uint8_t r = *position++;

        // apply pixelwise filter
        int sum = r + g + b;
        if (sum > 0){
            tmp = (float) ((float) (r) / ((float) (sum)));
            r = (uint8_t) (tmp*255);
        } else {
            r = 0;
        }

        //r-chroma intensity grayscale image
        *position2++ = r;
        *position2++ = r;
        *position2++ = r;

        //apply binary mask
        if(r>=threshold){
            *position3++ = (uint8_t) 255;
            *position3++ = (uint8_t) 255;
            *position3++ = (uint8_t) 255;
        } else {
            *position3++ = (uint8_t) 0;
            *position3++ = (uint8_t) 0;
            *position3++ = (uint8_t) 0;
        }

        // if images are side by side, adjust overlap
        // if (i!=0 && i%fb->width == 0){
        //     position = position3;
        //     position2 = position + offset;
        //     position3 = position2 + offset;
        // }
    }

    // convert to jpeg for streaming
    if(!fmt2jpg(rgb_buf,3*rgb_buf_len,fb->width,3*fb->height,PIXFORMAT_RGB888,90,_jpg_buf,_jpg_buf_len))
    {
        log_e("Convert to JPEG failed");
        success = false;
    }

    if(rgb_buf){
        heap_caps_free(rgb_buf);
        rgb_buf = NULL;
    }

    return success;
}