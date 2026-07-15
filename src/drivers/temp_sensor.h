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
 *  Integer-only path (no float): reads raw ticks and converts to
 *  centidegrees Celsius, so the firmware pulls in no soft-float.
 *
 *  @param centiC Output temperature in hundredths of a degree Celsius
 *  @return true when a value was read successfully
 */
bool tempReadRoomCenti(int16_t &centiC);

/*  @brief Read the board temperature from the STS40-AD1B (0x44).
 *
 *  @param centiC Output temperature in hundredths of a degree Celsius
 *  @return true when a value was read successfully
 */
bool tempReadBoardCenti(int16_t &centiC);

#endif // DRIVERS_TEMP_SENSOR_H
