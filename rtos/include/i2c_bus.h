#pragma once

#include "driver/i2c_master.h"

// one i2c bus that every i2c sensor shares (imu now, tof later probably)
void i2c_bus_start(void);

// for adding a device with i2c_master_bus_add_device
i2c_master_bus_handle_t i2c_bus(void);
