#include "pin_definitions.h"
#include "actuator_macros.h"

actuator_outputs_t saidas_last;

void init_actuators(){
    pinMode(CAM_WRITE_P, OUTPUT); // deteccao laranja

    saidas_last.cam = LOW; //Câmera Enable
};

void update_actuators(actuator_outputs_t saidas){
    digitalWrite(CAM_WRITE_P, saidas.cam); //deteccao de laranja

    saidas_last = saidas; //armazena estado atual das saídas
};