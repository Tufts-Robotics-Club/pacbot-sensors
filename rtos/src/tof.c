#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_timer.h"

#include "tof.h"

// size 1 so we always just grab the newest value
static QueueHandle_t tof_mailbox;

// fake values currently
static void tof_read_all(uint16_t out[NUM_TOF])
{
    for (int i = 0; i < NUM_TOF; i++) {
        out[i] = 150;
    }
}

static void tof_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();
    tof_data_t d;

    while (1) {
        tof_read_all(d.mm);
        d.t_us = esp_timer_get_time();
        xQueueOverwrite(tof_mailbox, &d);

        // keeps a steady 100 Hz even if reading the sensors takes a while
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000 / TOF_HZ));
    }
}

void tof_start(void)
{
    tof_mailbox = xQueueCreate(1, sizeof(tof_data_t));
    xTaskCreatePinnedToCore(tof_task, "tof", STACK_TOF, NULL, PRIO_TOF, NULL, RT_CORE);
}

bool tof_latest(tof_data_t *out)
{
    return xQueuePeek(tof_mailbox, out, 0) == pdTRUE;
}
