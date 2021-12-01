#if !defined(ACTUATOR_MACROS_H)
#define ACTUATOR_MACROS_H

    #include <Arduino.h>

    // estutura de dados para o conjunto de leituras dos sensores
    typedef struct {
        bool sole11 = LOW; //Solenóide 1 - Avanço
        bool sole12 = LOW; //Solenóide 1 - Retorno

        bool sole21 = LOW; //Solenóide 2 - Retorno
        bool sole22 = LOW; //Solenóide 2 - Avanço
        
        bool cam = LOW; //Câmera Enable

        bool motor11 = LOW; //Motor1 Pino1
        bool motor12 = LOW; //Motor1 Pino2
        int motor1_vel = 0; //Duty cicle motor 1

        // não é uma saída física de verdade, mas simplifica a vida se eu considerar como uma
        bool reset_encoder = true; //Ação de resetar o encoder 
    } actuator_outputs_t;

    void init_actuators();
    void update_actuators(actuator_outputs_t saidas);

#endif // ACTUATOR_MACROS_H