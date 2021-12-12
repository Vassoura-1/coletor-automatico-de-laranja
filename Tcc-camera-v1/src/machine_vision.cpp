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

/*bool shape_recognition2(const uint8_t width, const uint8_t height, uint8_t * vision_buf,
                        size_t vision_buf_len, uint8_t * circle_buff, const size_t circle_buff_len){
    uint8_t n_circles_inserted = 0;
    const uint8_t max_n_circles = ((int)circle_buff_len)/4 + 1;

    const uint8_t n_pixels_segment = 20;
    unsigned short index = 0;
    
    std::vector<uint8_t> radius_v, centerx_v, centery_v;

    while(index < vision_buf_len){
        std::vector<uint8_t> points_x, points_y;
        int n_points = 0;

        if(vision_buf[index]==2){
            // found start of the perimeter

            int x0,y0; //coordenadas (y = linha, x = coluna, indice = y * largura + x)
            y0 = ((int) index / width); //divisão inteira
            x0 = ((int) index % width); // resto de divisão
            points_x.push_back((uint8_t) x0); points_y.push_back((uint8_t) y0); n_points++;
            
            vision_buf[index] = 3; //marca pixel como contorno utilizado
            
            // Começa o algoritmo de Theo Pavlidis
            PA_direction_t direction = North; 
            PA_direction_t next_dir, p1_dir;
            int x, y;
            x=x0; y=y0;
            int n_giros = 0;

            int x1, y1, x2, y2, x3, y3;

            log_i("index %u, x0 %u, y0 %u", index, x0, y0);
            do {
                vTaskDelay(1);
                switch (direction) {
                default:
                case North:
                    x1 = x-1; y1 = y-1;
                    x2 = x; y2 = y-1;
                    x3 = x+1; y3 = y-1;
                    next_dir = West;
                    p1_dir = East;
                    break;
                case West:
                    x1 = x+1; y1 = y-1;
                    x2 = x+1; y2 = y;
                    x3 = x+1; y3 = y+1;
                    next_dir = South;
                    p1_dir = North;
                    break;
                case South:
                    x1 = x+1; y1 = y+1;
                    x2 = x; y2 = y+1;
                    x3 = x-1; y3 = y+1;
                    next_dir = East;
                    p1_dir = West;
                    break;
                case East:
                    x1 = x-1; y1 = y+1;
                    x2 = x-1; y2 = y;
                    x3 = x-1; y3 = y-1;
                    next_dir = North;
                    p1_dir = South;
                    break;
                }

                if ((x1>0 && x1<width) && (y1>0 && y1<height)) {
                    if(vision_buf[y1*width+x1] == 2){ //novo pixel de contorno
                        points_x.push_back((uint8_t) x1); points_y.push_back((uint8_t) y1); n_points++;
                        vision_buf[y1*width + x1] = 3; //marca como pixel visitado
                        x = x1; y = y1; n_giros = 0;
                        direction = p1_dir;
                        continue;
                    } else if (vision_buf[y1*width+x1] == 3) { //já visitado anteriormente
                        x = x1; y = y1; n_giros = 0;
                        direction = p1_dir;
                        continue;
                    }
                }
                if ((x2>0 && x2<width) && (y2>0 && y2<height)) {
                    if(vision_buf[y2*width+x2] == 2){ //novo pixel de contorno
                        points_x.push_back((uint8_t) x2); points_y.push_back((uint8_t) y2); n_points++;
                        vision_buf[y2*width + x2] = 3; //marca como pixel visitado
                        x = x2; y = y2; n_giros = 0;
                        continue;
                    } else if (vision_buf[y2*width+x2] == 3) { //já visitado anteriormente
                        x = x2; y = y2; n_giros = 0;
                        continue;
                    }
                }
                if ((x3>0 && x3<width) && (y3>0 && y3<height)) {
                    if(vision_buf[y3*width+x3] == 2){ //novo pixel de contorno
                        points_x.push_back((uint8_t) x3); points_y.push_back((uint8_t) y3); n_points++;
                        vision_buf[y3*width + x3] = 3; //marca como pixel visitado
                        x = x3; y = y3; n_giros = 0;
                        continue;
                    } else if (vision_buf[y3*width+x3] == 3) { //já visitado anteriormente
                        x = x3; y = y3; n_giros = 0;
                        continue;
                    }
                }

                direction = next_dir;
                n_giros++;
            } while (n_giros < 3 && (points_x.size() == 1 || x!=x0 || y!=y0));
            int segments = n_points / n_pixels_segment;

            std::vector<uint8_t>::iterator it_x = points_x.begin(), it_y = points_y.begin();

            for(int s = 0; s < segments; s++){
                log_i("Segment %d", s);
                vTaskDelay(1);
                {   
                    using namespace Eigen;
                    // Monta sistema Ax = y
                    Matrix<float, n_pixels_segment, 3> A;
                    VectorXf Y(n_pixels_segment);
                    Vector3f X;

                    for (int i = 0; i<n_pixels_segment; i++) {
                        A(i,all) << *it_x++, *it_y++, -1;
                    }

                    // Yi = xi^2 + yi^2
                    Y = A(all, 0).cwiseProduct(A(all, 0)) + A(all, 1).cwiseProduct(A(all,1));
                    // Ai = {2*xi, 2*yi, -1}
                    A(all,0) *= 2;
                    A(all,1) *= 2;
                    
                    //Resolve o sistema
                    // X << A.colPivHouseholderQr().solve(Y);
                    X << (A.transpose() * A).ldlt().solve(A.transpose() * Y)
                    log_v("x %f, y %f, alfa %f", X(0), X(1), X(2));
                    if (X(0)<0||X(1)<0) continue; //coordenadas dos centros não podem ser negativas
                    if (X(0)>=width||X(1)>=height) continue; //coordenadas dos centros não podem ser negativas
                    //pixel do centro não pode ter sido marcado como background
                    // if (!vision_buf[((uint8_t) X(1))*width+((uint8_t) X(0))]) continue; 

                    //calcula o raio a partir dos centros e alfa
                    float r;
                    r = X(0)*X(0)+X(1)*X(1)-X(2);
                    log_v("r^2 %f", r);
                    if (r<=0) continue; //previne contra raiz de número negativo
                    r = sqrt(r);
                    log_v("r %f", r);
                    if (r>(width/2)||r>(height/2)) continue; //raio não pode ser maior que metade do comprimento da imagem 

                    // converte para inteiros e armazena o centro
                    radius_v.push_back((uint8_t) r);
                    centerx_v.push_back((uint8_t) X(0));
                    centery_v.push_back((uint8_t) X(1));
                }
            }
        }

        index++;
    }

    // agrupamento de círculos baseado em proximidade e inserção no buffer de círculos
    int k = 0;
    log_i("Free heap: %d", ESP.getFreeHeap());
    log_i("Free PSRAM: %d", ESP.getFreePsram());
    log_i("radius_v.size() = %d", radius_v.size());
    if(radius_v.size() > 0)
    {
        while (n_circles_inserted < max_n_circles && k < radius_v.size())
        {
            bool grouped = false;
            
            for(int i=0; i < n_circles_inserted;i++){
                vTaskDelay(1);
                int d;
                d = (centerx_v[k]-circle_buff[4*i])*(centerx_v[k]-circle_buff[4*i]);
                d = (centery_v[k]-circle_buff[4*i+1])*(centery_v[k]-circle_buff[4*i+1]);
                d = sqrt(d);
                if(d<((circle_buff[4*i+2]*circle_buff[4*i+3])/(circle_buff[4*i+3]+1) + radius_v[k]/(circle_buff[4*i+3]+1))/5){
                    grouped = true;
                    uint8_t new_n = circle_buff[4*i+3] + 1;
                    circle_buff[4*i] = (uint8_t)((short) circle_buff[4*i]*(new_n-1) + centerx_v[k]) / new_n;
                    circle_buff[4*i+1] = (uint8_t)((short) circle_buff[4*i]*(new_n-1) + centery_v[k]) / new_n;
                    circle_buff[4*i+2] = (uint8_t)((short) circle_buff[4*i]*(new_n-1) + radius_v[k]) / new_n;
                    circle_buff[4*i+3] = new_n;
                    log_d("x %u, y %u, r %u, n %d, k %d", centerx_v[k], centery_v[k], radius_v[k], new_n, k);
                    break;
                }
            }
            if(!grouped){
                log_d("x %u, y %u, r %u, n %d", centerx_v[k], centery_v[k], radius_v[k], n_circles_inserted);
                circle_buff[4*n_circles_inserted] = centerx_v[k];
                circle_buff[4*n_circles_inserted+1] = centery_v[k];
                circle_buff[4*n_circles_inserted+2] = radius_v[k];
                circle_buff[4*n_circles_inserted+3] = 1;
                n_circles_inserted++;
            }
            k++;
        }
    }
    log_v("terminei shape");
    return true;
}
*/