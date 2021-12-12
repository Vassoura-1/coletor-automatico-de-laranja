#if !defined(SENSOR_MACROS_H)
#define SENSOR_MACROS_H

    #include <Arduino.h>

    // estutura de dados para o conjunto de leituras dos sensores
    typedef struct {
        bool cam_en = true; //flag que habilita detecção
        
        uint16_t d1 = 200; //sensor de distância 1
        uint16_t d2 = 200; //sensor de distância 2
    } sensor_readings_t;

    void init_sensors();
    void read_sensors(sensor_readings_t * readings);

#endif // SENSOR_MACROS_H