#ifndef APP_SERVO_CONTROL_H
#define APP_SERVO_CONTROL_H

#include <Arduino.h>

/*  @file app/servo_control.h
 *  @brief Servo feature seam over drivers/servo_out (PC6/PC7).
 *
 *  No Modbus addresses are assigned yet (see the reserved section in
 *  svc/modbus_map.h). When the feature lands: add the address constants,
 *  register handlers here, and drive drivers/servo_out — no other module
 *  needs to change.
 */

/*  @brief Register the servo Modbus handlers (none yet). */
void servoControlInit();

#endif // APP_SERVO_CONTROL_H
