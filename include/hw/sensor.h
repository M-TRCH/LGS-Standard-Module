#ifndef HW_SENSOR_H
#define HW_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2CSts4x.h>
#include "system.h"

/*  @file hw/sensor.h
 *  @brief Internal temperature sensor driver (Sensirion STS4x on I2C1).
 */

// STS4x temperature sensor on the internal I2C1 bus
extern SensirionI2CSts4x sts4x;

/*  @brief Initialize the internal I2C1 bus and the STS4x sensor. */
void sensorInit();

/*  @brief Read the internal temperature.
 *
 *  @param temperatureC Output temperature in degrees Celsius
 *  @return true when a value was read successfully
 */
bool sensorReadTemperature(float &temperatureC);

#endif // HW_SENSOR_H
