#include "decision_algorithm.h"
#include <cmath> /*sqrt*/

const float focal_length = 167.60; //mm

const uint16_t avanco_pistao = 300;   //mm
const uint16_t centro_de_pegada = 47; //mm

const uint16_t raio_regiao_de_captura = 100; //mm

const uint16_t raio_max_laranja = 45; //mm
const uint16_t raio_min_laranja = 25; //mm

bool decision_algorithm(const uint8_t width, const uint8_t height,
                        const uint8_t * circle_buff, const uint8_t circle_buff_len,
                        const uint16_t d1, const uint16_t d2, bool * fruit_detected_flag){
    bool success = true;
    bool detected = false;


    const uint16_t distance = d1;///2) + (d2/2); //mm
    const int max_n_circles = ((int)circle_buff_len)/4;

    const int center_x = width/2;
    const int center_y = height/2;

    if (distance <= (avanco_pistao+centro_de_pegada)){
        float scale_factor = ((float) distance) / focal_length; //pixels per mm

        for (int k = 0; k < max_n_circles; k++) {
            if(detected) break; //já detectou laranja que pode ser capturada
            
            if(!circle_buff[4*k+3]) continue; //sem circulo nessa posicao

            int x = circle_buff[4*k];
            int y = circle_buff[4*k+1];
            int r = circle_buff[4*k+2];

            log_v("x %d,y %d,r %d, n %d, %f", x, y, r, circle_buff[4*k+3], scale_factor);

            float center_center_distance = sqrt(pow(center_x - x,2) + pow(center_y - y,2));
            log_v("x_c %d, y_c %d, ccd %f", center_x,center_y, scale_factor*center_center_distance);
            //circulo fora da regiao de captura
            if((scale_factor*center_center_distance) > raio_regiao_de_captura) continue;

            float r_scaled = scale_factor*r;
            log_v("r_scaled %f mm", r_scaled);
            //laranja grande ou peguena de mais para ser pega
            if(r_scaled > raio_max_laranja || r_scaled < raio_min_laranja) continue;

            //se chegou até aqui temos laranja capturável
            detected = true;
        }
    }

    if(detected){
        *fruit_detected_flag = true;
    }
    else{
        *fruit_detected_flag = false;
    }

    return success;
}