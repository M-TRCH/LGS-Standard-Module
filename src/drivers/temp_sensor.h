#ifndef DRIVERS_TEMP_SENSOR_H
#define DRIVERS_TEMP_SENSOR_H

#include <Arduino.h>

/*  @file drivers/temp_sensor.h
 *  @brief Board temperature sensors: two Sensirion STS40 on I2C1.
 *
 *  - STS40-CD1B-R3 @0x46: room (ambient) temperature -> Modbus reg 20
 *  - STS40-AD1B-R3 @0x44: board temperature          -> Modbus reg 21
 */

/*  @brief Initialize the internal I2C1 bus and both STS40 sensors. */
void tempSensorInit();

/*  @brief Read the room (ambient) temperature from the STS40-CD1B (0x46).
 *
 *  @param temperatureC Output temperature in degrees Celsius
 *  @return true when a value was read successfully
 */
bool tempReadRoom(float &temperatureC);

/*  @brief Read the board temperature from the STS40-AD1B (0x44).
 *
 *  @param temperatureC Output temperature in degrees Celsius
 *  @return true when a value was read successfully
 */
bool tempReadBoard(float &temperatureC);

#endif // DRIVERS_TEMP_SENSOR_H
