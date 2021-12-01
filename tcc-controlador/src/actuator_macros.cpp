#include "pin_definitions.h"
#include "actuator_macros.h"

// Propriedades do PWM
// https://randomnerdtutorials.com/esp32-dc-motor-l298n-motor-driver-control-speed-direction/
const int freq = 30000;
const int pwmChannel = 0;
const int resolution = 8;
int dutyCycle = 200;

actuator_outputs_t saidas_last;

void init_actuators(){
    // Solenoids
    // garra
    pinMode(SOLE11_P, OUTPUT);
    pinMode(SOLE12_P, OUTPUT);
    // avanço
    pinMode(SOLE21_P, OUTPUT);
    pinMode(SOLE22_P, OUTPUT);
    
    // ESP-CAM enable
    pinMode(CAM_ENABLE_P, OUTPUT);

    // Motor (Ponte H)
    pinMode(HMotor11_P, OUTPUT);
    // pinMode(HMotor12_P, OUTPUT);
    
    // //Setup PWM
    // pinMode(HMotor1Enable_P, OUTPUT);
    // ledcSetup(pwmChannel, freq, resolution);
    // ledcAttachPin(HMotor1Enable_P, pwmChannel);
    // //pra mudar o duty cicle do pwm
    // ledcWrite(pwmChannel, 0);

    saidas_last.sole11  = LOW; //Solenóide 1 - Avanço
    saidas_last.sole12  = LOW; //Solenóide 1 - Retorno
    saidas_last.sole21 = LOW; //Solenóide 2 - Retorno
    saidas_last.sole22 = LOW; //Solenóide 2 - Avanço
    
    saidas_last.cam = LOW; //Câmera Enable

    saidas_last.motor11 = LOW; //Motor1 Pino1
    saidas_last.motor12 = LOW; //Motor1 Pino2
    saidas_last.motor1_vel = 0; //Duty cicle motor 1
};

void update_actuators(actuator_outputs_t saidas){
    digitalWrite(SOLE11_P, saidas.sole11); //Solenóide 1 - Avanço
    digitalWrite(SOLE12_P, saidas.sole12); //Solenóide 1 - Retorno

    digitalWrite(SOLE21_P, saidas.sole21); //Solenóide 2 - Retorno
    digitalWrite(SOLE22_P, saidas.sole22); //Solenóide 2 - Avanço
    
    digitalWrite(CAM_ENABLE_P, saidas.cam); //Câmera Enable

    digitalWrite(HMotor11_P, saidas.motor11); //Motor1 Pino1
    // digitalWrite(HMotor12_P, saidas.motor12); //Motor1 Pino2
    // ledcWrite(pwmChannel, saidas.motor1_vel); //Duty cicle motor 1

    saidas_last = saidas; //armazena estado atual das saídas
};