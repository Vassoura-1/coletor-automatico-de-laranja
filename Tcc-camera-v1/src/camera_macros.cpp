#define CAMERA_MODEL_AI_THINKER

#include <Arduino.h>
#include <esp_system.h>
#include <esp_log.h>

#include <esp_camera.h>

#include "camera_macros.h"
#include "camera_pins.h"

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz =20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 1       //if more than one, i2s runs in continuous mode. Use only with JPEG
};


esp_err_t init_camera()
{
  // allocate smaller buffers if no psram
  if(!psramFound()){
    camera_config.frame_size = FRAMESIZE_QQVGA;
    camera_config.jpeg_quality = 16;
    camera_config.fb_count = 4;
  }

  // initialize the camera
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK)
  {
      log_e("Camera Init Failed");
      return err;
  }

  // best configurations for vision
  sensor_t * s = esp_camera_sensor_get();
  s->set_saturation(s, 0);
  s->set_whitebal(s, 0);
  s->set_awb_gain(s, 0);
  s->set_bpc(s, 1);
  s->set_gain_ctrl(s, 0);

  log_i("Camera Init Ok!");
  return ESP_OK;
}