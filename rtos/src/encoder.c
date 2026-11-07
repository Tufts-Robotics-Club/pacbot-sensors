#include <stdbool.h>

#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_err.h"

#include "config.h"
#include "encoder.h"

// the hardware counter is only 16 bits, accum_count keeps going past these
#define ENC_HIGH_LIMIT  32767
#define ENC_LOW_LIMIT  -32768

// one pulse counter unit per wheel, counted in hardware so no ticks get missed
static pcnt_unit_handle_t units[2];

// counts every edge of A and B (4 per pulse), same as ESP32Encoder's attachFullQuad
static pcnt_unit_handle_t encoder_setup(int pin_a, int pin_b)
{
    pcnt_unit_config_t unit_cfg = {
        .low_limit = ENC_LOW_LIMIT,
        .high_limit = ENC_HIGH_LIMIT,
        .flags.accum_count = true,
    };
    pcnt_unit_handle_t unit;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_cfg, &unit));

    // ignore pulses shorter than 1 us, theyre noise
    pcnt_glitch_filter_config_t filter = {.max_glitch_ns = 1000};
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(unit, &filter));

    // channel a counts edges on A and looks at B for direction, channel b the other way round
    pcnt_chan_config_t a_cfg = {.edge_gpio_num = pin_a, .level_gpio_num = pin_b};
    pcnt_chan_config_t b_cfg = {.edge_gpio_num = pin_b, .level_gpio_num = pin_a};
    pcnt_channel_handle_t chan_a, chan_b;
    ESP_ERROR_CHECK(pcnt_new_channel(unit, &a_cfg, &chan_a));
    ESP_ERROR_CHECK(pcnt_new_channel(unit, &b_cfg, &chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    // accum_count needs a watch point at each limit so the driver notices every overflow
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(unit, ENC_HIGH_LIMIT));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(unit, ENC_LOW_LIMIT));

    // same weak pull ups the arduino version turned on
    gpio_pullup_en(pin_a);
    gpio_pullup_en(pin_b);

    ESP_ERROR_CHECK(pcnt_unit_enable(unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(unit));
    ESP_ERROR_CHECK(pcnt_unit_start(unit));
    return unit;
}

void encoder_start(void)
{
    units[ENC_LEFT] = encoder_setup(LEFT_ENC_A, LEFT_ENC_B);
    units[ENC_RIGHT] = encoder_setup(RIGHT_ENC_A, RIGHT_ENC_B);
}

int32_t encoder_count(enc_side_t side)
{
    int c = 0;
    pcnt_unit_get_count(units[side], &c);

    bool reversed = (side == ENC_LEFT) ? LEFT_ENC_REVERSED : RIGHT_ENC_REVERSED;
    return reversed ? -c : c;
}
