#if !defined(STATUS_SERVER_H)
#define STATUS_SERVER_H

#include <Arduino.h>
#include <WebServer.h>
#include <SPIFFS.h>

#include <esp_http_server.h>
#include <esp_timer.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "state_machine.h"
#include "sensor_macros.h"
#include "actuator_macros.h"

void startStatusServer();

#endif // STATUS_SERVER_H
