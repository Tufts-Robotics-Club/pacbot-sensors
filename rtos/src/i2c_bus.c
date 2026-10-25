#include "esp_err.h"

#include "config.h"
#include "i2c_bus.h"

static i2c_master_bus_handle_t bus;

void i2c_bus_start(void)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        // qwiic boards already have pull ups these just help a little bit
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &bus));
}

i2c_master_bus_handle_t i2c_bus(void)
{
    return bus;
}
