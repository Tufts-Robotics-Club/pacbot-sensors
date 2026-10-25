#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_task_wdt.h"
#include "driver/gptimer.h"

#include "config.h"
#include "motor.h"
#include "tof.h"

// size 1 so a new command replaces the old one
static QueueHandle_t cmd_mailbox;

static TaskHandle_t motor_task_handle;

// real motor code goes here
static void motors_stop(void)
{
    // TODO
}

// timer interrupt every 1 ms that wakes up motor_task
static bool IRAM_ATTR motor_timer_isr(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *ctx)
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(motor_task_handle, &woken); // wake up motor task
    return woken == pdTRUE;
}

static void motor_timer_start(void)
{

    // setting up the timer
    gptimer_handle_t timer;
    gptimer_config_t cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&cfg, &timer));

    // attatching the function to the interrupt
    gptimer_event_callbacks_t cbs = {.on_alarm = motor_timer_isr};
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer, &cbs, NULL));

    gptimer_alarm_config_t alarm = {
        .alarm_count = 1000000 / MOTOR_HZ,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    // enable the timer
    ESP_ERROR_CHECK(gptimer_set_alarm_action(timer, &alarm));
    ESP_ERROR_CHECK(gptimer_enable(timer));
    ESP_ERROR_CHECK(gptimer_start(timer));
}

static void motor_task(void *arg)
{
    // set up the timer here so its interrupt runs on core 1 not the wifi core
    motor_timer_start();
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    tof_data_t tof = {0};
    char cmd[MSG_LEN];

    while (1) {
        // sleep until the timer wakes us up (from the ISR)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        esp_task_wdt_reset();

        tof_latest(&tof);

        // receive takes it out of the mailbox so we never run the same command twice
        if (xQueueReceive(cmd_mailbox, cmd, 0) == pdTRUE) {
            // TODO: figure out what to do with commands once we know what the ai sends
            // reply with comms_send() if the computer needs an answer
        }

        motors_stop();
    }
}

void motor_start(void)
{
    cmd_mailbox = xQueueCreate(1, MSG_LEN);
    xTaskCreatePinnedToCore(motor_task, "motor", STACK_MOTOR, NULL, PRIO_MOTOR, &motor_task_handle, RT_CORE);
}

void motor_command(const char *text)
{
    char buf[MSG_LEN];
    strlcpy(buf, text, sizeof(buf));
    xQueueOverwrite(cmd_mailbox, buf);
}
