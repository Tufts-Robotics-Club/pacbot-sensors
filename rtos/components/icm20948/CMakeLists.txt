# chip code from gopmur/icm20948 1.0.1 (MIT, based on sparkfuns icm-20948 library)
# left out its i2c/spi files since they use the old esp-idf i2c driver. our i2c code is in src/imu.c
idf_component_register(SRCS "icm20948.c" "icm20948_dmp.c"
                       INCLUDE_DIRS ".")
