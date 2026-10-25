#include "esp_err.h"
#include "nvs_flash.h"

#include "i2c_bus.h"
#include "tof.h"
#include "imu.h"
#include "motor.h"
#include "comms.h"
#include "stats.h"

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // tasks to be run
    i2c_bus_start(); // core 0
    tof_start(); // core 1
    imu_start(); // core 1
    motor_start(); // core 1
    comms_start(); // core 0
    stats_start(); // core 0
}
