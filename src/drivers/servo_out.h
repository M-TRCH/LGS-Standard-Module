#ifndef DRIVERS_SERVO_OUT_H
#define DRIVERS_SERVO_OUT_H

#include <Arduino.h>

/*  @file drivers/servo_out.h
 *  @brief Servo driver for the two board servo outputs (PC6, PC7).
 */

#define SERVO_COUNT     2
#define SERVO_MIN_ANGLE 0
#define SERVO_MAX_ANGLE 180

/*  @brief Attach both servo outputs to their pins. */
void servoInit();

/*  @brief Move a servo to an angle.
 *  @param index Servo index (0-based, < SERVO_COUNT)
 *  @param angle Target angle in degrees (clamped to [SERVO_MIN_ANGLE, SERVO_MAX_ANGLE])
 */
void servoWrite(uint8_t index, uint8_t angle);

/*  @brief Drive a servo with a raw pulse width.
 *  @param index        Servo index (0-based, < SERVO_COUNT)
 *  @param microseconds Pulse width in microseconds
 */
void servoWriteMicroseconds(uint8_t index, uint16_t microseconds);

/*  @brief Detach a servo output (stops sending pulses). */
void servoDetach(uint8_t index);

#endif // DRIVERS_SERVO_OUT_H
