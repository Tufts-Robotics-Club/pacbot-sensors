#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_task_wdt.h"
#include "driver/gptimer.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "config.h"
#include "motor.h"
#include "encoder.h"
#include "comms.h"
#include "tof.h"

// size 1 so a new command replaces the old one
static QueueHandle_t cmd_mailbox;

static TaskHandle_t motor_task_handle;

// indexed by ENC_LEFT / ENC_RIGHT. two PWM channels per motor, one on each L293D input
static const int in1_pins[2] = {LEFT_IN1, RIGHT_IN1};
static const int in2_pins[2] = {LEFT_IN2, RIGHT_IN2};
static const ledc_channel_t in1_channels[2] = {LEDC_CHANNEL_0, LEDC_CHANNEL_1};
static const ledc_channel_t in2_channels[2] = {LEDC_CHANNEL_2, LEDC_CHANNEL_3};

#define PWM_MAX 255   // 8 bit duty, same as the arduino version

// master on/off for both motors. off = L293D outputs float so the motors coast
static void motors_enable(bool on)
{
    gpio_set_level(LEFT_EN, on);
    gpio_set_level(RIGHT_EN, on);
}

static void pwm_channel_start(int pin, ledc_channel_t channel)
{
    ledc_channel_config_t ch_cfg = {
        .gpio_num = pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = channel,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,   // both inputs low = brake
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
}

static void motor_pins_start(void)
{
    // EN starts low (the pull downs hold it there during boot too) until the PWM is set up
    gpio_config_t en_cfg = {
        .pin_bit_mask = (1ULL << LEFT_EN) | (1ULL << RIGHT_EN),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&en_cfg));
    motors_enable(false);

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = MOTOR_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    for (int i = 0; i < 2; i++) {
        pwm_channel_start(in1_pins[i], in1_channels[i]);
        pwm_channel_start(in2_pins[i], in2_channels[i]);
    }

    motors_enable(true);
}

static void set_channel(ledc_channel_t channel, int percent)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, percent * PWM_MAX / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
}

// signed duty in percent, -100..100. negative = backward, 0 = brake
// the PWM goes on one input and the other stays low, so the off part of each cycle brakes instead of coasting
static void set_motor(enc_side_t side, int duty)
{
    if (duty > 100) duty = 100;
    if (duty < -100) duty = -100;

    // drop the old side to 0 first so both inputs are never high at the same time
    if (duty >= 0) {
        set_channel(in1_channels[side], 0);
        set_channel(in2_channels[side], duty);
    } else {
        set_channel(in2_channels[side], 0);
        set_channel(in1_channels[side], -duty);
    }
}

static void motors_stop(void)
{
    set_motor(ENC_LEFT, 0);
    set_motor(ENC_RIGHT, 0);
}

// commands from console.py, same as the arduino version. one number = both wheels, two = left right
//   d <duty%>   open loop duty, -100..100 (turns the motors back on if x turned them off)
//   s or 0      stop with the brake
//   x           master off, EN low so the motors coast
static void handle_command(const char *cmd)
{
    char reply[MSG_LEN];
    int left = 0, right = 0;
    int n = sscanf(cmd + 1, "%d %d", &left, &right);
    if (n == 1) right = left;

    if (cmd[0] == 'd' && n >= 1) {
        set_motor(ENC_LEFT, left);
        set_motor(ENC_RIGHT, right);
        motors_enable(true);
        snprintf(reply, sizeof(reply), "duty set to L=%d%% R=%d%%", left, right);
    } else if (cmd[0] == 's' || cmd[0] == '0') {
        motors_stop();
        snprintf(reply, sizeof(reply), "stopped");
    } else if (cmd[0] == 'x') {
        motors_stop();
        motors_enable(false);
        snprintf(reply, sizeof(reply), "motors off");
    } else {
        snprintf(reply, sizeof(reply), "unknown command: %.100s", cmd);
    }
    comms_send(reply);
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

// prints the encoder counts 5 times a second while were testing them
// its own low priority task on core 0 so printing never slows down motor_task
static void enc_print_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(200));
        printf("L: %ld  R: %ld\n", (long)encoder_count(ENC_LEFT), (long)encoder_count(ENC_RIGHT));
    }
}

static void motor_task(void *arg)
{
    // set up the encoders and timer here so their interrupts run on core 1 not the wifi core
    motor_pins_start();
    encoder_start();
    if (PRINT_ENC) {
        xTaskCreatePinnedToCore(enc_print_task, "enc_print", STACK_STATS, NULL, PRIO_STATS, NULL, COMMS_CORE);
    }
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
            // TODO: switch to whatever the ai sends once we know
            handle_command(cmd);
        }
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
