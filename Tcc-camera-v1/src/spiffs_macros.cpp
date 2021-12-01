#include <Arduino.h>

#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <esp_spiffs.h>

#include "spiffs_macros.h"

esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

void init_spiffs(){
    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            log_d("Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            log_d("Failed to find SPIFFS partition");
        } else {
            log_d("Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        log_d("Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        log_d("Partition size: total: %d, used: %d", total, used);
    }
}