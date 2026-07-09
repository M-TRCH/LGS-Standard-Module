#ifndef HAL_SENSOR_H
#define HAL_SENSOR_H

#include <Arduino.h>
#include "system.h"

/*
 * hal_sensor.h - Hardware abstraction for the on-board environment sensor
 * -----------------------------------------------------------------------
 * Wraps the Sensirion STS4x temperature sensor on I2C1 so the application
 * logic depends on a plain temperature API instead of the sensor driver.
 */

/* @brief Initialize the I2C bus and temperature sensor (call once at startup). */
void sensorInit();

/* @brief Read the board temperature in degrees Celsius.
 * @param temperatureC Output: measured temperature in Celsius.
 * @return true on success, false if the measurement failed. */
bool sensorReadTemperature(float &temperatureC);

#endif // HAL_SENSOR_H
