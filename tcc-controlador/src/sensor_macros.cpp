// #define USE_LOAD_CELL
// #define USE_DIST

#include "pin_definitions.h"
#include "sensor_macros.h"

#include <Arduino.h>

#include <Wire.h>
#include <VL53L0X.h>
#include <NewPing.h>
#include <HX711.h>
#include <ESP32Encoder.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Ultrassounds
#define ULTRASSOM_N_MEDICOES 3U
#define ULTRASSOM_MAX_DIST 25U //(cm)
NewPing ultra1(US1_TRIG_P, US1_ECHO_P, ULTRASSOM_MAX_DIST); //10ms timeout
// NewPing ultra2(US2_TRIG_P, US2_ECHO_P, ULTRASSOM_MAX_DIST); //10ms timeout
const long soundspeed = 331.3; // m/s (pode ser melhorado com medições de temperatura e humidade)

// Distance sensor
// #define VL53L0X_LONG_RANGE
// #define VL53L0X_HIGH_SPEED
// #define VL53L0X_HIGH_ACCURACY
VL53L0X d_sensor1;
VL53L0X d_sensor2;
const uint8_t d_sensor1_adrr = 61; // 7-bit address
const uint8_t d_sensor2_adrr = 72; // 7-bit address

// Load Cell
HX711 scale;

// Encoder
ESP32Encoder encoder;

void init_distance_sensors(){
    // Reset Distance Sensors
    pinMode(XShut1_P, OUTPUT);
    pinMode(XShut2_P, OUTPUT);
    digitalWrite(XShut1_P, LOW);
    digitalWrite(XShut2_P, LOW);

    // Begin I2C bus
    Wire.begin(SPI_SDA_P,SPI_SCL_P);

    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Begin distance sensors
    digitalWrite(XShut1_P, HIGH);
    d_sensor1.setTimeout(500);
    d_sensor1.setAddress(d_sensor1_adrr);
    if (!d_sensor1.init())
        log_e("\n Erro ao inicializar sensor de distância 1 (endereco: %u)", d_sensor1_adrr);
    else
        log_i("Sensor d1 inicializado com sucesso. Endereço: %u", d_sensor1.getAddress());    
    digitalWrite(XShut2_P, HIGH);
    d_sensor2.setTimeout(500);
    d_sensor2.setAddress(d_sensor2_adrr);
    if (!d_sensor2.init())
        log_e("\n Erro ao inicializar sensor de distância 2 (endereco: %u)", d_sensor2_adrr);
    else
        log_i("Sensor d2 inicializado com sucesso. Endereço: %u", d_sensor2.getAddress());

    #ifdef VL53L0X_LONG_RANGE
    // lower the return signal rate limit (default is 0.25 MCPS)
    d_sensor1.setSignalRateLimit(0.1);
    d_sensor2.setSignalRateLimit(0.1);
    // increase laser pulse periods (defaults are 14 and 10 PCLKs)
    d_sensor1.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
    d_sensor1.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
    d_sensor2.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
    d_sensor2.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
    #endif

    #ifdef VL53L0X_HIGH_SPEED
    // reduce timing budget to 20 ms (default is about 33 ms)
    d_sensor1.setMeasurementTimingBudget(20000);
    d_sensor2.setMeasurementTimingBudget(20000);
    #elif defined VL53L0X_HIGH_ACCURACY
    // increase timing budget to 200 ms
    d_sensor1.setMeasurementTimingBudget(200000);
    d_sensor2.setMeasurementTimingBudget(200000);
    #endif
}

void init_load_cell(){
    // Begin Load Cell 
    scale.begin(HX711_DT_P, HX711_SCK_P);
    scale.set_scale(2280.f); //TODO
    scale.tare();
}

void init_sensors(){
    // Start Switch
    pinMode(START_P, INPUT_PULLDOWN);
    // ESP-CAM read
    pinMode(CAM_READ_P, INPUT_PULLDOWN);
    // Reed Switches
    pinMode(LIMIT11_P, INPUT); //normalmente fechado
    pinMode(LIMIT12_P, INPUT); //normalmente fechado
    pinMode(LIMIT21_P, INPUT); //normalmente fechado
    pinMode(LIMIT22_P, INPUT);
    //Encoder
    // Enable the weak pull up resistors
	ESP32Encoder::useInternalWeakPullResistors=UP;
    encoder.attachHalfQuad(ENC_DT_P,ENC_CLK_P);
    encoder.clearCount();
    encoder.setFilter(1023);

    // Ultrassons não precisam de begin()
    log_i("Sensores digitais prontos");

    #ifdef USE_DIST
        init_distance_sensors();
        log_i("Sensores de distância prontos");
    #endif

    #ifdef USE_LOAD_CELL
        init_load_cell();
        log_i("Célula de carga pronto");
    #endif
};

void read_sensors(sensor_readings_t * readings, bool reset_encoder){
    log_v("função read_sensors");
    
    // atualizar leituras
    readings->stop11= !digitalRead(LIMIT11_P); //normalmente fechado
    readings->stop12= !digitalRead(LIMIT12_P); //normalmente fechado
    readings->stop21= !digitalRead(LIMIT21_P); //normalmente fechado
    readings->stop22= digitalRead(LIMIT22_P);

    readings->u1    = (ultra1.ping_median(ULTRASSOM_N_MEDICOES)/20000.0)*soundspeed;
    #ifdef USE_DIST
        readings->d1    = d_sensor1.readRangeSingleMillimeters();
        readings->d2    = d_sensor2.readRangeSingleMillimeters();
    #endif
    // readings->u2    = (ultra2.ping_median(ULTRASSOM_N_MEDICOES)/20000.0)*soundspeed;


    readings->cam   = digitalRead(CAM_READ_P);
    readings->start = digitalRead(START_P);

    int encoder_position = encoder.getCount();
    if (reset_encoder && (encoder_position != 0)){
        encoder_position = 0;
        encoder.clearCount();;
    }

    readings->encpos= encoder_position;
};