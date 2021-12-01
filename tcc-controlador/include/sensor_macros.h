#if !defined(SENSOR_MACROS_H)
#define SENSOR_MACROS_H

    #include <Arduino.h>

    // estutura de dados para o conjunto de leituras dos sensores
    typedef struct {
        bool stop11 = false; //fim de curso 11 (active low)
        bool stop12 = false; //fim de curso 12 (active low)
        bool stop21 = false; //fim de curso 21 (active low)
        bool stop22 = false; //fim de curso 22

        bool start = false; //interruptor de modo de colheita
        
        bool cam = false; //flag de laranja detectada
        
        uint u1 = 100; //ultrassom 1
        uint u2 = 100; //ultrassom 2
        
        uint16_t d1 = 100; //sensor de distância 1
        uint16_t d2 = 100; //sensor de distância 2
        
        int encpos; //posicao do encoder
    } sensor_readings_t;

    void init_sensors();
    void read_sensors(sensor_readings_t * readings, bool reset_encoder);

#endif // SENSOR_MACROS_H