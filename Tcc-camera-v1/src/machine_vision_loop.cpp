#include "machine_vision_loop.h"

#include <esp_timer.h>
#include <esp_camera.h>
#include <img_converters.h>

#include <vector>
#include <list>
#include <cmath> /*sqrt*/

#include <ArduinoEigenDense.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

bool color_filtering(camera_fb_t *fb, uint8_t *vision_buf, const size_t vision_buf_len, uint8_t threshold)
{
    bool success = true;
    uint8_t *rgb_buf = (uint8_t *)heap_caps_malloc(3 * vision_buf_len, MALLOC_CAP_SPIRAM);
    // get rgb buffer
    if (fb->format != PIXFORMAT_RGB888)
    {
        if (!fmt2rgb888(fb->buf, fb->len, fb->format, rgb_buf))
        {
            log_e("Convert to RGB888 failed");
            success = false;
            return success;
        }
    }
    else
    {
        log_e("PIXFORMAT must not be RGB888");
        success = false;
        return success;
    }

    float tmp = 0;
    uint8_t *position = rgb_buf;
    uint8_t *position2 = vision_buf;
    // image data bytes are stored as a trail of BGR bytes read from left to right, top to bottom
    for (int i = 0; i < vision_buf_len; i += 1)
    {
        // access pixel color bytes
        uint8_t b = *position++;
        uint8_t g = *position++;
        uint8_t r = *position++;

        // apply pixelwise filter
        int sum = r + g + b;
        if (sum > 0)
        {
            tmp = (float)((float)(r) / ((float)(sum)));
            r = (uint8_t)(tmp * 255);
        }
        else
        {
            r = 0;
        }

        // apply binary mask
        if (r >= threshold)
        {
            *position2++ = (uint8_t)2;
        }
        else
        {
            *position2++ = (uint8_t)0;
        }
        // log_i("pos i=%d",i);
    }
    heap_caps_free(rgb_buf);

    return success;
}

bool edge_detection(const uint8_t width, const uint8_t height,
                    uint8_t *vision_buf, const size_t vision_buf_len)
{
    bool success = true;
    unsigned short i = -1;
    uint8_t *tmp_buff = (uint8_t *)malloc(vision_buf_len);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            i++;
            if (!vision_buf[i])
            { // skip background pixels
                tmp_buff[i] = 0;
                continue;
            }
            if (x > 0 && (!vision_buf[i - 1]))
            {
                tmp_buff[i] = vision_buf[i];
                continue;
            }
            if (x < (width - 1) && (!vision_buf[i + 1]))
            {
                tmp_buff[i] = vision_buf[i];
                continue;
            }
            if (y > 0 && (!vision_buf[i - width]))
            {
                tmp_buff[i] = vision_buf[i];
                continue;
            }
            if (y < height - 1 && !vision_buf[i + width])
            {
                tmp_buff[i] = vision_buf[i];
                continue;
            }
            tmp_buff[i] = 1;
        }
    }

    memcpy(vision_buf, tmp_buff, vision_buf_len);
    free(tmp_buff);
    tmp_buff = NULL;

    return success;
}

/*bool shape_recognition(const uint8_t width, const uint8_t height, uint8_t * vision_buf,
                        size_t vision_buf_len, const uint8_t max_label,
                        uint8_t * circle_buff, const size_t circle_buff_len,
                        int n_masked){
    using PII = std::pair<uint8_t, uint8_t>;
    bool success = true;
    uint8_t n_circles_inserted = 0;
    const uint8_t max_n_circles = ((int)circle_buff_len)/4 + 1;

    const uint8_t image_center_x = width/2;
    const uint8_t image_center_y = height/2;
    const uint8_t n_pixels_segment = 10;

    for (uint8_t label = 2; label <= max_label; label++){
        unsigned short index = 0;
        uint8_t x_looking = image_center_x, y_looking = image_center_y;
        bool found = false;

        if(!success) break;

        // look for the start of the perimeter
        // while (vision_buf[index] != label)
        // {
        //     index++;
        //     if (index==vision_buf_len){
        //         success = false;
        //         break;
        //     }
        //     if (vision_buf[index] == label){
        //         found = true;
        //     }
        // }

        // look for the start of the perimeter
        // while (!found && x_looking < width) {
        //     index = y_looking*width+x_looking;
        //         if(vision_buf[index]==label){
        //             found = true;
        //         }
        //     x_looking++;
        // }
        // x_looking = image_center_x;
        // while (!found && x_looking > 0) {
        //     index = y_looking*width+x_looking;
        //         if(vision_buf[index]==label){
        //             found = true;
        //         }
        //     x_looking--;
        // }
        // x_looking = image_center_x;
        // while (!found && y_looking < height) {
        //     index = y_looking*width+x_looking;
        //         if(vision_buf[index]==label){
        //             found = true;
        //         }
        //     y_looking++;
        // }
        // y_looking = image_center_y;
        // while (!found && y_looking > 0) {
        //     index = y_looking*width+x_looking;
        //         if(vision_buf[index]==label){
        //             found = true;
        //         }
        //     y_looking--;
        // }

        // if (!found){
        //     success = false;
        //     break;
        // }

        if (success)
        {
            // found start of the perimeter
            // trace it in a clockwise manner and record the coordinates
            uint8_t x0,y0; //coordenadas (y = linha, x = coluna, indice = y * largura + x)
            y0 = (uint8_t) ((int) index / width); //divis??o inteira
            x0 = (uint8_t) ((int) index % width); // resto de divis??o

            uint8_t x, y;
            x=x0; y=y0;
            int n_points = 0;

            // log_v("index %u, width %u, x0 %u, y0 %u", index, width, x0, y0);

            vTaskDelay(10);
            std::vector<uint8_t> points_x, points_y;
            // bool inserted = false;
            // do {
            //     vTaskDelay(1);
            //     points_x.push_back(x); points_y.push_back(y); n_points++;
            //     inserted = false;
            //     std::list<PII> CW_LIST;
            //     if(x>0 && x < (width-1)){
            //         if (y>0 && y < (height-1)){
            //             CW_LIST.insert(CW_LIST.begin(),
            //                                 {{x+1,y+1},{x+1,y},{x+1,y-1},
            //                                 {x,y-1},{x-1,y-1},{x-1,y},
            //                                 {x-1,y+1},{x,y+1}});
            //         } else if (y>0) {
            //             std::list<PII> TMP_L({{x+1,y},{x+1,y-1},{x,y-1},
            //                                 {x-1,y-1},{x-1,y}});
            //             CW_LIST.insert(CW_LIST.begin(),TMP_L.begin(),TMP_L.end());
            //         } else {
            //             std::list<PII> TMP_L({{x+1,y+1},{x+1,y},{x-1,y},
            //                                 {x-1,y+1},{x,y+1}});
            //             CW_LIST.insert(CW_LIST.begin(),TMP_L.begin(),TMP_L.end());
            //         }
            //     } else if (x>0) {
            //         if (y>0 && y < (height-1)){
            //             std::list<PII> TMP_L({{x,y-1},{x-1,y-1},{x-1,y},
            //                                 {x-1,y+1},{x,y+1}});
            //             CW_LIST.insert(CW_LIST.begin(),TMP_L.begin(),TMP_L.end());
            //         } else if (y>0) {
            //             std::list<PII> TMP_L({{x,y-1},{x-1,y-1},{x-1,y}});
            //             CW_LIST.insert(CW_LIST.begin(),TMP_L.begin(),TMP_L.end());
            //         } else {
            //             std::list<PII> TMP_L({{x-1,y},{x-1,y+1},{x,y+1}});
            //             CW_LIST.insert(CW_LIST.begin(),TMP_L.begin(),TMP_L.end());
            //         }
            //     } else {
            //         if (y>0 && y < (height-1)){
            //             std::list<PII> TMP_L({{x+1,y+1},{x+1,y},{x+1,y-1},
            //                                 {x,y-1},{x,y+1}});
            //             CW_LIST.insert(CW_LIST.begin(),TMP_L.begin(),TMP_L.end());
            //         } else if (y>0) {
            //             std::list<PII> TMP_L({{x+1,y},{x+1,y-1},{x,y-1}});
            //             CW_LIST.insert(CW_LIST.begin(),TMP_L.begin(),TMP_L.end());
            //         } else {
            //             std::list<PII> TMP_L({{x+1,y+1},{x+1,y},{x,y+1}});
            //             CW_LIST.insert(CW_LIST.begin(),TMP_L.begin(),TMP_L.end());
            //         }
            //     }
            //     for (auto i : CW_LIST)
            //     {
            //         bool in_list = false;
            //         uint8_t x_i = i.first; uint8_t y_i = i.second;
            //         if(vision_buf[y_i*width+x_i] == label){
            //             for(int k=0; k < points_x.size(); k++){
            //                 if (x_i == points_x[k] && y_i == points_y[k]){
            //                     in_list = true;
            //                 }
            //             }
            //             if(!in_list)
            //             {
            //                 x = x_i; y = y_i;
            //                 inserted = true; break;
            //             }
            //         }
            //     }
            // } while (inserted && (x != x0 || y != y0));

            for(int i = 0; i < vision_buf_len; i++){
                if(vision_buf[i]==label){
                    x = (uint8_t) ((int) i % width); // resto de divis??o
                    y = (uint8_t) ((int) i / width); //divis??o inteira
                    points_x.push_back(x); points_y.push_back(y); n_points++;
                }
            }

            int segments = n_points / n_pixels_segment;
            // log_i("cheguei aqui: segments %d", segments);

            std::vector<uint8_t> radius_v, centerx_v, centery_v;
            std::vector<uint8_t>::iterator it_x = points_x.begin(), it_y = points_y.begin();

            for(int s = 0; s < segments; s++){
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
                    X << A.colPivHouseholderQr().solve(Y);
                    log_v("x %f, y %f, alfa %f", X(0), X(1), X(2));
                    if (X(0)<0||X(1)<0) continue; //coordenadas dos centros n??o podem ser negativas
                    //pixel do centro n??o pode ter sido marcado como background
                    if (!vision_buf[((uint8_t) X(1))*width+((uint8_t) X(0))]) continue;

                    //calcula o raio a partir dos centros e alfa
                    float r;
                    r = X(0)*X(0)+X(1)*X(1)-X(2);
                    log_v("r^2 %f", r);
                    if (r<=0) continue; //previne contra raiz de n??mero negativo
                    r = sqrt(r);
                    log_v("r %f", r);
                    if (r>(width/2)||r>(height/2)) continue; //raio n??o pode ser maior que metade do comprimento da imagem

                    // converte para inteiros e armazena o centro
                    radius_v.push_back((uint8_t) r);
                    centerx_v.push_back((uint8_t) X(0));
                    centery_v.push_back((uint8_t) X(1));
                }
            }
            // agrupamento de c??rculos baseado em proximidade e inser????o no buffer de c??rculos
            int k = 0;

            log_v("cheguei aqui2: radius_v.size() = %d", radius_v.size());
            log_v("Free heap: %d", ESP.getFreeHeap());
            log_v("Free PSRAM: %d", ESP.getFreePsram());
            if(radius_v.size() > 0)
            {
                log_i("radius_v.size() = %d", radius_v.size());
                while (n_circles_inserted < max_n_circles && k < radius_v.size())
                {
                    bool grouped = false;

                    for(int i=0; i < n_circles_inserted;i++){
                        vTaskDelay(1);
                        int d;
                        d = (centerx_v[k]-circle_buff[4*i])*(centerx_v[k]-circle_buff[4*i]);
                        d = (centery_v[k]-circle_buff[4*i+1])*(centery_v[k]-circle_buff[4*i+1]);
                        d = sqrt(d);
                        if(d<((circle_buff[4*i+2]*circle_buff[4*i+3])/(circle_buff[4*i+3]+1) + radius_v[k]/(circle_buff[4*i+3]+1))/2){
                            grouped = true;
                            uint8_t new_n = circle_buff[4*i+3] + 1;
                            circle_buff[4*i] = (uint8_t)((short) circle_buff[4*i]*(new_n-1) + centerx_v[k]) / new_n;
                            circle_buff[4*i+1] = (uint8_t)((short) circle_buff[4*i]*(new_n-1) + centery_v[k]) / new_n;
                            circle_buff[4*i+2] = (uint8_t)((short) circle_buff[4*i]*(new_n-1) + radius_v[k]) / new_n;
                            circle_buff[4*i+3] = new_n;
                        }
                    }
                    if(!grouped){
                        log_i("x %u, y %u, r %u, n %d", centerx_v[k], centery_v[k], radius_v[k], n_circles_inserted);
                        circle_buff[4*n_circles_inserted] = centerx_v[k];
                        circle_buff[4*n_circles_inserted+1] = centery_v[k];
                        circle_buff[4*n_circles_inserted+2] = radius_v[k];
                        circle_buff[4*n_circles_inserted+3] = 1;
                        n_circles_inserted++;
                    }
                    k++;
                }
            }
            log_v("cheguei aqui3, label %d, (Max label: %d)", label, max_label);
        } else {
            log_e("Error: label %d not found in buffer (Max label: %d)", label, max_label);
            success = true;
            // break;
        }
    }

    log_v("cheguei aqui4");
    return true;
}
*/

typedef enum
{
    North,
    West,
    South,
    East
} PA_direction_t;

const uint8_t n_pixels_segment = 20;

void interpolate(const uint8_t width, const uint8_t height,
                    uint8_t * points_x, uint8_t * points_y, std::vector<uint8_t> & radius_v,
                    std::vector<uint8_t> & centerx_v, std::vector<uint8_t> & centery_v){
    
    using namespace Eigen;
    // Monta sistema Ax = y
    Matrix<float, n_pixels_segment, 3> A;
    VectorXf Y(n_pixels_segment);
    Vector3f X;

    for (int i = 0; i < n_pixels_segment; i++)
    {
        A(i, all) << points_x[i], points_y[i], -1;
    }

    // Yi = xi^2 + yi^2
    Y = A(all, 0).cwiseProduct(A(all, 0)) + A(all, 1).cwiseProduct(A(all, 1));
    // Ai = {2*xi, 2*yi, -1}
    A(all, 0) *= 2;
    A(all, 1) *= 2;

    // Resolve o sistema
    //  X << A.colPivHouseholderQr().solve(Y);
    X << (A.transpose() * A).ldlt().solve(A.transpose() * Y) log_v("x %f, y %f, alfa %f", X(0), X(1), X(2));
    if (X(0) < 0 || X(1) < 0)
        return; // coordenadas dos centros n??o podem ser negativas
    if (X(0) >= width || X(1) >= height)
        return; // coordenadas dos centros n??o podem ser negativas
    // pixel do centro n??o pode ter sido marcado como background
    //  if (!vision_buf[((uint8_t) X(1))*width+((uint8_t) X(0))]) continue;

    // calcula o raio a partir dos centros e alfa
    float r;
    r = X(0) * X(0) + X(1) * X(1) - X(2);
    log_v("r^2 %f", r);
    if (r <= 0)
        return; // previne contra raiz de n??mero negativo
    r = sqrt(r);
    log_v("r %f", r);
    if (r > (width / 2) || r > (height / 2))
        return; // raio n??o pode ser maior que metade do comprimento da imagem

    // converte para inteiros e armazena o centro
    radius_v.push_back((uint8_t)r);
    centerx_v.push_back((uint8_t)X(0));
    centery_v.push_back((uint8_t)X(1));
    return;
};

bool shape_recognition2(const uint8_t width, const uint8_t height, uint8_t *vision_buf,
                        size_t vision_buf_len, uint8_t *circle_buff, const size_t circle_buff_len)
{
    uint8_t n_circles_inserted = 0;
    const uint8_t max_n_circles = ((int)circle_buff_len) / 4 + 1;

    unsigned short index = 1;

    std::vector<uint8_t> radius_v, centerx_v, centery_v;
    uint8_t points_x[n_pixels_segment] = {}, points_y[n_pixels_segment] = {};

    while (index < vision_buf_len)
    {
        int n_points = 0;

        if (!vision_buf[index-1] && vision_buf[index] == 2)
        {
            // found start of the perimeter

            int x0, y0;                // coordenadas (y = linha, x = coluna, indice = y * largura + x)
            y0 = ((int)index / width); // divis??o inteira
            x0 = ((int)index % width); // resto de divis??o
            n_points = 0;
            points_x[n_points] = (uint8_t)x0;
            points_y[n_points] = (uint8_t)y0;
            n_points++;

            vision_buf[index] = 3; // marca pixel como contorno utilizado

            // Come??a o algoritmo de Theo Pavlidis
            PA_direction_t direction = North;
            PA_direction_t next_dir, p1_dir;
            int x, y;
            x = x0;
            y = y0;
            int n_giros = 0; bool moved = false;

            int x1, y1, x2, y2, x3, y3;

            // log_i("index %u, x0 %u, y0 %u", index, x0, y0);
            do {
                // log_i("pav: x0 %u, y0 %u, x %d, y %d", x0, y0, x,y);
                vTaskDelay(1);
                if (n_points == n_pixels_segment)
                {
                    // log_i("Entrei na interpolacao");

                    interpolate(width, height, points_x, points_y, radius_v, centerx_v, centery_v);
                    
                    n_points = 0;
                    // log_i("Sai na interpolacao");
                }

                if(x != x0 || y != y0) moved = true;

                switch (direction)
                {
                    default:
                    case North:
                        x1 = x - 1; y1 = y - 1;
                        x2 = x; y2 = y - 1;
                        x3 = x + 1; y3 = y - 1;
                        next_dir = West;
                        p1_dir = East;
                        break;
                    case West:
                        x1 = x + 1; y1 = y - 1;
                        x2 = x + 1; y2 = y;
                        x3 = x + 1; y3 = y + 1;
                        next_dir = South;
                        p1_dir = North;
                        break;
                    case South:
                        x1 = x + 1; y1 = y + 1;
                        x2 = x; y2 = y + 1;
                        x3 = x - 1; y3 = y + 1;
                        next_dir = East;
                        p1_dir = West;
                        break;
                    case East:
                        x1 = x - 1; y1 = y + 1;
                        x2 = x - 1; y2 = y;
                        x3 = x - 1; y3 = y - 1;
                        next_dir = North;
                        p1_dir = South;
                        break;
                }

                if ((x1 >= 0 && x1 < width) && (y1 >= 0 && y1 < height))
                {
                    if (vision_buf[y1 * width + x1] == 2)
                    { // novo pixel de contorno
                        points_x[n_points] = (uint8_t)x1;
                        points_y[n_points] = (uint8_t)y1;
                        n_points++;
                        vision_buf[y1 * width + x1] = 3; // marca como pixel visitado
                        x = x1;
                        y = y1;
                        n_giros = 0;
                        direction = p1_dir;
                        continue;
                    }
                    else if (vision_buf[y1 * width + x1] == 3)
                    { // j?? visitado anteriormente
                        x = x1;
                        y = y1;
                        n_giros = 0;
                        direction = p1_dir;
                        continue;
                    }
                }
                if ((x2 >= 0 && x2 < width) && (y2 >= 0 && y2 < height))
                {
                    if (vision_buf[y2 * width + x2] == 2)
                    { // novo pixel de contorno
                        points_x[n_points] = (uint8_t)x2;
                        points_y[n_points] = (uint8_t)y2;
                        n_points++;
                        vision_buf[y2 * width + x2] = 3; // marca como pixel visitado
                        x = x2;
                        y = y2;
                        n_giros = 0;
                        continue;
                    }
                    else if (vision_buf[y2 * width + x2] == 3)
                    { // j?? visitado anteriormente
                        x = x2;
                        y = y2;
                        n_giros = 0;
                        continue;
                    }
                }
                if ((x3 >= 0 && x3 < width) && (y3 >= 0 && y3 < height))
                {
                    if (vision_buf[y3 * width + x3] == 2)
                    { // novo pixel de contorno
                        points_x[n_points] = (uint8_t)x3;
                        points_y[n_points] = (uint8_t)y3;
                        n_points++;
                        vision_buf[y3 * width + x3] = 3; // marca como pixel visitado
                        x = x3;
                        y = y3;
                        n_giros = 0;
                        continue;
                    }
                    else if (vision_buf[y3 * width + x3] == 3)
                    { // j?? visitado anteriormente
                        x = x3;
                        y = y3;
                        n_giros = 0;
                        continue;
                    }
                }

                direction = next_dir;
                n_giros++;
            } while (n_giros < 3 && (!moved || x != x0 || y != y0));
            // log_i("sai do pav");
        }
        index++;
    }

    // agrupamento de c??rculos baseado em proximidade e inser????o no buffer de c??rculos
    int k = 0;
    // log_i("Free heap: %d", ESP.getFreeHeap());
    // log_i("Free PSRAM: %d", ESP.getFreePsram());
    // log_i("radius_v.size() = %d", radius_v.size());
    if (radius_v.size() > 0)
    {
        while (n_circles_inserted < max_n_circles && k < radius_v.size())
        {
            bool grouped = false;

            for (int i = 0; i < n_circles_inserted; i++)
            {
                vTaskDelay(1);
                int d;
                d = (centerx_v[k] - circle_buff[4 * i]) * (centerx_v[k] - circle_buff[4 * i]);
                d = (centery_v[k] - circle_buff[4 * i + 1]) * (centery_v[k] - circle_buff[4 * i + 1]);
                d = sqrt(d);
                if (d < ((circle_buff[4 * i + 2] * circle_buff[4 * i + 3]) / (circle_buff[4 * i + 3] + 1) + radius_v[k] / (circle_buff[4 * i + 3] + 1)) / 5)
                {
                    grouped = true;
                    uint8_t new_n = circle_buff[4 * i + 3] + 1;
                    circle_buff[4 * i] = (uint8_t)((short)circle_buff[4 * i] * (new_n - 1) + centerx_v[k]) / new_n;
                    circle_buff[4 * i + 1] = (uint8_t)((short)circle_buff[4 * i] * (new_n - 1) + centery_v[k]) / new_n;
                    circle_buff[4 * i + 2] = (uint8_t)((short)circle_buff[4 * i] * (new_n - 1) + radius_v[k]) / new_n;
                    circle_buff[4 * i + 3] = new_n;
                    log_d("x %u, y %u, r %u, n %d, k %d", centerx_v[k], centery_v[k], radius_v[k], new_n, k);
                    break;
                }
            }
            if (!grouped)
            {
                log_d("x %u, y %u, r %u, n %d", centerx_v[k], centery_v[k], radius_v[k], n_circles_inserted);
                circle_buff[4 * n_circles_inserted] = centerx_v[k];
                circle_buff[4 * n_circles_inserted + 1] = centery_v[k];
                circle_buff[4 * n_circles_inserted + 2] = radius_v[k];
                circle_buff[4 * n_circles_inserted + 3] = 1;
                n_circles_inserted++;
            }
            k++;
        }
    }
    log_v("terminei shape");
    return true;
}