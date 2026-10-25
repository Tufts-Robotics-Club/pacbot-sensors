#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "icm20948.h"

#include "config.h"
#include "i2c_bus.h"
#include "imu.h"

static const char *TAG = "imu";

// size 1 so we always just grab the newest value
static QueueHandle_t imu_mailbox;

static i2c_master_dev_handle_t imu_dev;
static icm20948_device_t icm;
static bool imu_ok;

#define ACCEL_RANGE       GPM_4    // +-4 g
#define GYRO_RANGE        DPS_500  // +-500 deg/s
#define ACCEL_PER_G       8192.0f  // comes from the datasheet for +-4 g
#define GYRO_PER_DPS      65.5f    // comes from the datasheet for +-500 deg/s

// the library calls these to talk to the imu (register number always goes first)
static icm20948_status_e imu_write(uint8_t reg, uint8_t *data, uint32_t len, void *user)
{
    i2c_master_transmit_multi_buffer_info_t parts[] = {
        {.write_buffer = &reg, .buffer_size = 1 },
        {.write_buffer = data, .buffer_size = len },
    };
    return i2c_master_multi_buffer_transmit(imu_dev, parts, 2, 50) == ESP_OK ? ICM_20948_STAT_OK : ICM_20948_STAT_ERR;
}

static icm20948_status_e imu_i2c_read(uint8_t reg, uint8_t *data, uint32_t len, void *user)
{
    return i2c_master_transmit_receive(imu_dev, &reg, 1, data, len, 50) == ESP_OK ? ICM_20948_STAT_OK : ICM_20948_STAT_ERR;
}

static const icm20948_serif_t imu_serif = {
    .write = imu_write,
    .read = imu_i2c_read,
};

// runs once to add the imu to our bus and set it up
static void imu_setup(void)
{
    i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = IMU_ADDR,
        .scl_speed_hz = I2C_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus(), &dev, &imu_dev));

    icm20948_init_struct(&icm);
    icm20948_link_serif(&icm, &imu_serif);

    // dont want a loose wire crashing the whole robot so we just print it and skip the imu
    if (icm20948_check_id(&icm) != ICM_20948_STAT_OK) {
        ESP_LOGE(TAG, "nothing answering at 0x%02x", IMU_ADDR);
        return;
    }

    // start from a known state
    icm20948_sw_reset(&icm);
    vTaskDelay(pdMS_TO_TICKS(250));

    // has to be awake first or it ignores the settings
    icm20948_sleep(&icm, false);
    icm20948_low_power(&icm, false);

    icm20948_internal_sensor_id_bm sensors = ICM_20948_INTERNAL_ACC | ICM_20948_INTERNAL_GYR;
    icm20948_set_sample_mode(&icm, sensors, SAMPLE_MODE_CONTINUOUS);

    icm20948_fss_t range = { .a = ACCEL_RANGE, .g = GYRO_RANGE };
    icm20948_set_full_scale(&icm, sensors, range);

    // cuts out noise above ~50 Hz
    icm20948_dlpcfg_t filter = { .a = ACC_D50BW4_N68BW8, .g = GYR_D51BW2_N73BW3 };
    icm20948_set_dlpf_cfg(&icm, sensors, filter);
    icm20948_enable_dlpf(&icm, ICM_20948_INTERNAL_ACC, true);
    icm20948_enable_dlpf(&icm, ICM_20948_INTERNAL_GYR, true);

    // readings are in bank 0 and nothing else uses the imu
    icm20948_set_bank(&icm, 0);

    imu_ok = true;
    ESP_LOGI(TAG, "icm-20948 ready at 0x%02x", IMU_ADDR);
}

// each reading is 2 bytes with the high byte first, this glues them into one signed number
static int16_t bytes_to_int16(const uint8_t *b)
{
    return (int16_t)((b[0] << 8) | b[1]);
}

// false if the imu isnt there or the read failed
static bool imu_read(imu_data_t *out)
{
    // accel xyz then gyro xyz
    // get everything in one read
    uint8_t b[12];
    if (!imu_ok || imu_i2c_read(AGB0_REG_ACCEL_XOUT_H, b, sizeof(b), NULL) != ICM_20948_STAT_OK) {
        return false;
    }

    for (int i = 0; i < 3; i++) {
        out->accel[i] = bytes_to_int16(&b[i * 2]) / ACCEL_PER_G;
        out->gyro[i] = bytes_to_int16(&b[6 + i * 2]) / GYRO_PER_DPS;
    }
    return true;
}

static void imu_task(void *arg)
{
    imu_setup();

    TickType_t last_wake = xTaskGetTickCount();
    imu_data_t d;

    while (1) {
        if (imu_read(&d)) {
            d.t_us = esp_timer_get_time();
            xQueueOverwrite(imu_mailbox, &d);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000 / IMU_HZ));
    }
}

void imu_start(void)
{
    imu_mailbox = xQueueCreate(1, sizeof(imu_data_t));
    xTaskCreatePinnedToCore(imu_task, "imu", STACK_IMU, NULL, PRIO_IMU, NULL, RT_CORE);
}

bool imu_latest(imu_data_t *out)
{
    return xQueuePeek(imu_mailbox, out, 0) == pdTRUE;
}
