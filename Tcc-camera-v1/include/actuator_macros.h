#if !defined(ACTUATOR_MACROS_H)
#define ACTUATOR_MACROS_H

    #include <Arduino.h>

    // estutura de dados para o conjunto de leituras dos sensores
    typedef struct {
        bool cam = LOW; //Laranja Detectada
    } actuator_outputs_t;

    void init_actuators();
    void update_actuators(actuator_outputs_t saidas);

#endif // ACTUATOR_MACROS_H