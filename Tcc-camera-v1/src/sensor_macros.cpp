// #define USE_LOAD_CELL

#include "pin_definitions.h"
#include "sensor_macros.h"

#include <Arduino.h>

#include <Wire.h>
#include <VL53L0X.h>

// Distance sensor
#define VL53L0X_LONG_RANGE
#define VL53L0X_HIGH_SPEED
// #define VL53L0X_HIGH_ACCURACY
VL53L0X d_sensor1;
VL53L0X d_sensor2;
const uint8_t d_sensor1_adrr = 61; // 7-bit address
const uint8_t d_sensor2_adrr = 72; // 7-bit address

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
    // d_sensor2.setAddress(d_sensor2_adrr);
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

void init_sensors(){
    // ESP-EN read
    pinMode(CAM_ENABLE_P, INPUT_PULLDOWN);

    init_distance_sensors();
    log_i("Sensores de distância prontos");
};

void read_sensors(sensor_readings_t * readings){
    log_v("função read_sensors");

    readings->d1 = d_sensor1.readRangeSingleMillimeters();
    readings->d2 = d_sensor2.readRangeSingleMillimeters();

    // readings->cam_en = digitalRead(CAM_ENABLE_P);
};