#include "esp_camera.h"

#ifndef _MACHINE_VISION_H_
#define _MACHINE_VISION_

// Processa uma imagem aplicando um filtro de cor pixel a pixel. Pra isso, a função converte 
// o frame original pra uint8. Também toma como um argumento ponteiros para um buffer jpeg e 
// o tamanho do buffer resultante, onde serão armazenados a máscara após a aplicação do filtro.
//
// Retorna true on success
bool process_image_buffer(camera_fb_t *fb, uint8_t **_jpg_buf, size_t *_jpg_buf_len, uint8_t threshold = 108);

#endif