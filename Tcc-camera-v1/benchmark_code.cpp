void loop()
{ 
    log_d("Enter loop");
    log_d("Free heap: %d", ESP.getFreeHeap());
    // log_d("Free PSRAM: %d", ESP.getFreePsram());
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    // Benchmark 
    
    camera_fb_t * fb = esp_camera_fb_get();

    size_t rgb_buf_len = fb->height*fb->width*3;
    uint8_t * rgb_buf = (uint8_t *) malloc(rgb_buf_len*3);
    int offset = rgb_buf_len;
    // bool success = true;
    
    // get rgb buffer
    if(fb->format != PIXFORMAT_RGB888){
        if(!fmt2rgb888(fb->buf, fb->len, fb->format, rgb_buf)){
            log_e("Convert to RGB888 failed");
            // success = false;
        }
    } else {
        //for images on top of each other
        memcpy(rgb_buf,fb->buf,fb->len);
    }

    log_d("Convert to RBG888 success");

    int masked = 0;
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
        if(r>=108){
            *position3++ = (uint8_t) 255;
            *position3++ = (uint8_t) 255;
            *position3++ = (uint8_t) 255;
            masked++;
        } else {
            *position3++ = (uint8_t) 0;
            *position3++ = (uint8_t) 0;
            *position3++ = (uint8_t) 0;
        }
    }
    // size_t _jpg_buf_len = 0;
    // uint8_t * _jpg_buf = NULL;

    // fmt2jpg(rgb_buf, rgb_buf_len*3, fb->width, 3*fb->height, PIXFORMAT_RGB888, 80, &_jpg_buf, &_jpg_buf_len);

    // heap_caps_free(_jpg_buf);
    // _jpg_buf = NULL;
    
    // log average frames per second
    int64_t fr_end = esp_timer_get_time();        
    int64_t frame_time = fr_end - last_frame;
    last_frame = fr_end;
    frame_time /= 1000;
    uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);
    log_i("RBG888: %ums (%.1ffps), AVG: %ums (%.1ffps)\n",
            (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time,
            avg_frame_time, 1000.0 / avg_frame_time
        );

    log_i("Masked pixels: %d\n", masked);

    esp_camera_fb_return(fb);

    free(rgb_buf);
    rgb_buf = NULL;

    // vTaskDelay(30000 / portTICK_RATE_MS);
}