#include "esp_camera.h"

#ifndef _MACHINE_VISION_LOOP_H_
#define _MACHINE_VISION_LOOP_H_
    
    #include <Arduino.h>

    // Processa uma imagem aplicando um filtro de cor pixel a pixel. Pra isso, a função converte 
    // o frame original pra uint8. Também toma como um argumento ponteiros para um buffer jpeg e 
    // o tamanho do buffer resultante, onde serão armazenados a máscara após a aplicação do filtro.
    //
    // Retorna true on success
    bool color_filtering(camera_fb_t *fb, uint8_t * vision_buf,
                        const size_t vision_buf_len , uint8_t threshold = 108);
    
    bool edge_detection(const uint8_t width, const uint8_t height, 
                        uint8_t * vision_buf, const size_t vision_buf_len);

    bool shape_recognition(const uint8_t width, const uint8_t height, uint8_t * vision_buf,
                        const size_t vision_buf_len, const uint8_t max_label,
                        uint8_t * circle_buff, const size_t circle_buff_len,
                        int n_masked);

    bool shape_recognition2(const uint8_t width, const uint8_t height,
                        uint8_t * vision_buf, const size_t vision_buf_len,
                        uint8_t * circle_buff, const size_t circle_buff_len);

    bool decision_algorithm(uint8_t * circle_buff, uint8_t circle_buff_len,
                            bool * fruit_detected_flag);
#endif