#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_timer.h"

#include "config.h"
#include "imu.h"
#include "stats.h"

// prints cpu % for each task and how close each stack got to running out
static void print_cpu(void)
{
    static char buf[1024];
    vTaskGetRunTimeStats(buf);
    printf("\n%-16s%12s%8s\n%s", "task", "abs time", "cpu%", buf);
    printf("free stack (bytes): motor %u  tof %u  imu %u  reply %u  stats %u\n",
           (unsigned)uxTaskGetStackHighWaterMark(xTaskGetHandle("motor")),
           (unsigned)uxTaskGetStackHighWaterMark(xTaskGetHandle("tof")),
           (unsigned)uxTaskGetStackHighWaterMark(xTaskGetHandle("imu")),
           (unsigned)uxTaskGetStackHighWaterMark(xTaskGetHandle("reply")),
           (unsigned)uxTaskGetStackHighWaterMark(NULL));
    printf("free heap: %u  min ever: %u\n\n",
           (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size());
}

static void print_imu(void)
{
    imu_data_t d;
    if (!imu_latest(&d)) {
        printf("imu: no data\n");
        return;
    }
    printf("imu: accel %6.3f %6.3f %6.3f g   gyro %7.2f %7.2f %7.2f deg/s   age %lld us\n",
           d.accel[0], d.accel[1], d.accel[2],
           d.gyro[0], d.gyro[1], d.gyro[2],
           esp_timer_get_time() - d.t_us);
}

static void stats_task(void *arg)
{
    int ticks = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(200));

        if (PRINT_IMU) {
            print_imu();
        }

        // cpu report every 5 s (25 x 200 ms)
        if (++ticks == 25) {
            ticks = 0;
            print_cpu();
        }
    }
}

void stats_start(void)
{
    xTaskCreatePinnedToCore(stats_task, "stats", STACK_STATS, NULL, PRIO_STATS, NULL, COMMS_CORE);
}
