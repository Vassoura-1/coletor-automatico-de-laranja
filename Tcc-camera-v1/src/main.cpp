#define USE_SPIFFS
#define USE_WIFI
#define USE_CAMERA_SERVER
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

#include <numeric>

#include "utilities.h"
#include "camera_macros.h"
#include "ota_server.h"
#include "machine_vision_loop.h"
#include "labelling.h"
#include "decision_algorithm.h"

#include "actuator_macros.h"
#include "sensor_macros.h"

const char* ssid = "WifiDoTcc_AP2";
const char* password = "tccdalaranja";

// external functions
esp_err_t init_camera();
void startCameraServer();
void stopCameraServer();

void startOtaServer();
void stopOtaServer();

ra_filter_t * ra_filter_init(ra_filter_t * filter, size_t sample_size);
int ra_filter_run(ra_filter_t * filter, int value);

//filtro de média móvel
static ra_filter_t ra_filter;
static int64_t last_frame = 0;

// machine vision flag
bool vision_enabled = false;

#define FRAMESIZE_DEF FRAMESIZE_QQVGA
#define FB_HEIGHT 120
#define FB_WIDTH 160
#define VISION_THRESHOLD 120
#define MAX_COMPONENT_SIZE 3000
#define MIN_COMPONENT_SIZE 500
#define MAX_CIRCLES 40

// machine vision buffer
const size_t vision_buf_len = FB_HEIGHT*FB_WIDTH;
uint8_t * vision_buf = (uint8_t *) malloc(vision_buf_len);//,MALLOC_CAP_SPIRAM);
// RGB picture buffer
const size_t rgb_buf_len = vision_buf_len*3;
uint8_t * rgb_buf = (uint8_t *) malloc(rgb_buf_len);//,MALLOC_CAP_SPIRAM);

// buffer to hold circle groups;
const size_t circle_buf_len = MAX_CIRCLES*4;
uint8_t * circle_buf = (uint8_t *) calloc(circle_buf_len,1);

// auxiliary variables
int n_masked = 0;
sensor_readings_t leituras;
actuator_outputs_t saidas;
bool fruit_detected_flag = false;

camera_fb_t * fb = NULL; 
void setup()
{
    Serial.begin(115200);
    log_d("Total heap: %d", ESP.getHeapSize());
    log_d("Free heap: %d", ESP.getFreeHeap());

    // Initialize camera
    init_camera();
    sensor_t * s = esp_camera_sensor_get();
    s->set_framesize(s, FRAMESIZE_DEF);

    #ifdef USE_SPIFFS
      // Initialize SPIFFS file system
      if(!SPIFFS.begin(true)){
        log_e("An Error has occurred while mounting SPIFFS");
        return;
      }
    #endif

    #ifdef USE_WIFI
      // Connect to wifi
      WiFi.softAP(ssid, password);
      log_i("ESP IP is: %s", WiFi.softAPIP().toString().c_str());
      // Start OTA server
      startOtaServer();
      #ifdef USE_CAMERA_SERVER
        // Start camera configuration server
        startCameraServer();
        log_i("HTTP Server ready");
      #endif
    #endif

    #ifdef USE_SENSORS
      init_sensors();
    #endif

    #ifdef USE_ACTUATORS
      init_actuators();
    #endif

    // Filtro de média móvel para benchmarks
    ra_filter_init(&ra_filter, 10);

    log_i("Free heap: %d", ESP.getFreeHeap());
    log_i("Free PSRAM: %d", ESP.getFreePsram());
}


void loop() // Loop runs on core 1, with priority 1
{ 
    // reset watchdog
    // vTaskDelay(1000);
    vTaskDelay(10);

    log_d("Free heap: %d", ESP.getFreeHeap());
    log_d("Free PSRAM: %d", ESP.getFreePsram());

    #ifdef USE_SENSORS
      read_sensors(&leituras);
    #endif

    if(leituras.cam_en) {
      n_masked = 0;
      // loop time for logging
      if(!last_frame) {
          last_frame = esp_timer_get_time();
      }

      // get picture
      fb = esp_camera_fb_get();
      if (!fb) {
        log_e("Camera capture failed");
        return;
      }
      
      // apply color filtering
      if(!color_filtering(fb, rgb_buf, rgb_buf_len, 
                        vision_buf, vision_buf_len, VISION_THRESHOLD)){
        log_e("Error applying color filtering");
        esp_camera_fb_return(fb);
        return;
      }
      
      // free picture frame buffer
      esp_camera_fb_return(fb);

      // apply size filtering
      uint8_t max_label = LabelImage(FB_WIDTH,FB_HEIGHT,vision_buf,vision_buf,MAX_COMPONENT_SIZE,MIN_COMPONENT_SIZE);
      if(!max_label || max_label == 255){
        log_e("Error applying size filtering");
        log_e("max_label = %d", max_label);
        return;
      }

      for(int i = 0; i<vision_buf_len; i++){
        vision_buf[i] = (vision_buf[i]>0)?2:0;
        // n_masked++;
      }
      // log_d("Masked pixels: %d\n", n_masked);
      max_label = 2;

      // apply edge detection
      if(!edge_detection(FB_WIDTH, FB_HEIGHT, vision_buf, vision_buf_len)){
        log_e("Error applying edge detection");
        return;
      }

      n_masked = 0;
      for(int i = 0; i<vision_buf_len; i++){
        if(vision_buf[i]==1) n_masked++;
      }
      log_d("contour pixels: %d\n", n_masked);


      // // apply shape recognition
      if(!shape_recognition(FB_WIDTH, FB_HEIGHT, vision_buf, vision_buf_len, max_label, circle_buf, circle_buf_len, n_masked)){
        log_e("Error applying shape recognition");
        return;
      }

      // apply decision algorithm
      if(!decision_algorithm(FB_WIDTH, FB_HEIGHT, circle_buf, circle_buf_len, leituras.d1, leituras.d2, &fruit_detected_flag)){
        log_e("Error applying decision algorithm");  
        memset(circle_buf,0,circle_buf_len);  
        return;
      }
      
      for (int i = 0; i < (circle_buf_len/4); i++)
      {
        if (circle_buf[4*i+3])
        {
          log_i("C.G %d: x %u, y %u, r %u, n %u", i+1,circle_buf[4*i],circle_buf[4*i+1],circle_buf[4*i+2],circle_buf[4*i+3]);
        }
      }

      // clear circle buffer
      memset(circle_buf,0,circle_buf_len);  

      saidas.cam = fruit_detected_flag;

      // change outputs
      #ifdef USE_ACTUATORS
        update_actuators(saidas);
      #endif

      
    //   // filter avg fps
    int64_t fr_end = esp_timer_get_time();        
    int64_t frame_time = fr_end - last_frame;
    last_frame = fr_end;
    frame_time /= 1000;
    uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);

    //   // log avg fps
    if(fruit_detected_flag){
      log_i("detected: %d time in loop: %ums (%.1ffps), AVG: %ums (%.1ffps)\n", fruit_detected_flag,
          (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time,
          avg_frame_time, 1000.0 / (uint32_t)avg_frame_time);
    }
  }
}