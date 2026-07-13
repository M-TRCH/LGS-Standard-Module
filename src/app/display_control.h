#ifndef APP_DISPLAY_CONTROL_H
#define APP_DISPLAY_CONTROL_H

#include <Arduino.h>

/*  @file app/display_control.h
 *  @brief Display feature seam: owns the OLED content policy driven by
 *         Modbus (reg 60 set-number, coils 1010/1011).
 *
 *  The addresses are registered and reserved; rendering is not
 *  implemented yet. Implementing the feature means filling in the
 *  handlers here — no other module needs to change.
 */

/*  @brief Register the Modbus handlers for the display addresses. */
void displayControlInit();

#endif // APP_DISPLAY_CONTROL_H
