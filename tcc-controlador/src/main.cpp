#define USE_SPIFFS
#define USE_WIFI
#define USE_SENSORS
#define USE_ACTUATORS

#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>

#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ota_server.h"
#include "state_machine.h"
#include "sensor_macros.h"
#include "actuator_macros.h"

#include "variaveis_globais.h"

const char* ssid = "WifidoTCC_AP1";
const char* password = "tccdalaranja";

sensor_readings_t leituras;
actuator_outputs_t saidas;
state_t current_state;
bool reset_encoder_flag;

void startOtaServer();
void stopOtaServer();

state_t initStateMachine();
state_t resetStateMachine();
state_t updateStateMachine();

void init_sensors();
void read_sensors();

void init_actuators();
void update_actuators();

void startStatusServer();

void setup() {
    Serial.begin(115200);

    log_d("Total heap: %d", ESP.getHeapSize());
    log_d("Free heap: %d", ESP.getFreeHeap());
    log_d("Free PSRAM: %d", ESP.getFreePsram());

    #ifdef USE_SPIFFS
      // Initialize SPIFFS file system
      if(!SPIFFS.begin(true)){
        Serial.println(F("An Error has occurred while mounting SPIFFS"));
        return;
      }
    #endif

    #ifdef USE_WIFI
      // Connect to wifi
      WiFi.softAP(ssid, password);
      log_i("ESP IP is: %s", WiFi.softAPIP().toString().c_str());
      // Start OTA server
      startOtaServer();
      startStatusServer();
    #endif
    
    #ifdef USE_SENSORS
      init_sensors();
    #endif

    #ifdef USE_ACTUATORS
      init_actuators();
    #endif

    current_state = initStateMachine();

    log_i("Free heap: %d", ESP.getFreeHeap());
}

void loop() // Loop runs on core 1, with priority 1 
{
  // available memmory
  // log_d("Free heap: %d", ESP.getFreeHeap());

  // process inputs
  #ifdef USE_SENSORS
    read_sensors(&leituras, reset_encoder_flag);
  #endif

  /* debug info of sensor readings
  log_d("d1 = %u", leituras.d1);
  log_d("d2 = %u", leituras.d2);
  log_d("cam = %d", leituras.cam);
  log_d("start = %d", leituras.start);
  log_d("u1 = %u", leituras.u1);
  log_d("u2 = %u", leituras.u2);
  log_d("encpos = %d", leituras.encpos);
  */

  // update state machine
  current_state = updateStateMachine(&leituras);

  // get outputs from state
  saidas = getOutPutsFromState();
  reset_encoder_flag = saidas.reset_encoder;

  /* debug info of state machine outputs
  log_d("sole1 = %d", saidas.sole1);
  log_d("sole21 = %d", saidas.sole21);
  log_d("sole22 = %d", saidas.sole22);
  log_d("cam = %d", saidas.cam);
  log_d("motor11 = %d", saidas.motor11);
  log_d("motor12 = %d", saidas.motor12);
  log_d("motor1_vel = %u", saidas.motor1_vel);
  log_d("reset_encoder = %u", saidas.reset_encoder);
  */

  // update outputs
  #ifdef USE_ACTUATORS
    update_actuators(saidas);
  #endif

  // reset watchdog
  vTaskDelay(10);
}