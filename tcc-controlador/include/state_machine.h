#if !defined(STATE_MACHINE_H)
#define STATE_MACHINE_H
    #include "Arduino.h"

    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>

    #include "sensor_macros.h"
    #include "actuator_macros.h"

    typedef enum {
    S0, // IDLE
    S1, // Escaneia
    S2, // Aproxima
    S3, // Agarra
    S4, // Verifica
    S5, // Volta
    S6, // Solta
    S7  // Volta rápido
    } state_t;

    state_t initStateMachine(void);
    state_t resetStateMachine(void);
    state_t updateStateMachine(sensor_readings_t * readings);
    actuator_outputs_t getOutPutsFromState(void);

#endif // STATE_MACHINE_H