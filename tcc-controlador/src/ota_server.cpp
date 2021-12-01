#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <ArduinoOTA.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ota_server.h"

static int port = 3232;
// bool otaIsUpdating = false;

//task to handle ota requests
static TaskHandle_t otaTask = NULL;
//task to handle ota requests
void handle_OTA_task( void* pvParameters) {
  for(;;){
    ArduinoOTA.handle();
    taskYIELD();
  }
}


//Initialize OTA server
void startOtaServer(void)
{
	ArduinoOTA
		// OTA request received
		.onStart([]() {
			String type;
			if (ArduinoOTA.getCommand() == U_FLASH)
				type = "sketch";
			else { // U_SPIFFS
				type = "filesystem";
                #ifdef USE_SPIFFS
                    SPIFFS.end();
                #endif
            }

			log_i("Start updating %s",type);
			// otaIsUpdating = true;
		})
		.onEnd([]() {
			// OTA is finished
			log_i("End");
			// otaIsUpdating = false;
		})
		.onProgress([](unsigned int progress, unsigned int total) {
			// Status report during OTA
    		log_i("Progress: %u%%\r", (progress / (total / 100)));
		})
		.onError([](ota_error_t error) {
			log_e("Error[%u]: ", error);
			if (error == OTA_AUTH_ERROR) log_e("Auth Failed");
			else if (error == OTA_BEGIN_ERROR) log_e("Begin Failed");
			else if (error == OTA_CONNECT_ERROR) log_e("Connect Failed");
			else if (error == OTA_RECEIVE_ERROR) log_e("Receive Failed");
			else if (error == OTA_END_ERROR) log_e("End Failed");
			// otaIsUpdating = false;
		});

	// Set OTA port
	ArduinoOTA.setPort(port);
    // Authentication
    ArduinoOTA.setPasswordHash("cdcbe012addd41cf4f6a49cd38569088");

	// Start the OTA server
	ArduinoOTA.begin();
    log_i("OTA server started at port: %d", port);

	// Create OTA handler task pinned to core 1
    xTaskCreatePinnedToCore( 
      handle_OTA_task, "OTA handler",
      10000, // task stack size in bytes
      NULL, 1, &otaTask, 1
    ); 
	log_i("OTA handler task assigned to core 1");
}

/**
 * Stop the OTA server
 */
void stopOtaServer(void)
{
	ArduinoOTA.end();
    log_i("OTA server stopped");
}