#ifndef APP_DISPLAY_CONTROL_H
#define APP_DISPLAY_CONTROL_H

#include <Arduino.h>

/*  @file app/display_control.h
 *  @brief OLED display policy driven by Modbus: reg 60 (number) + coil 1010
 *         (enable). While enabled, the number renders as the big two-digit
 *         font (the DEMO face); R5.0 renders 0-99 — out-of-range writes to
 *         reg 60 clamp to 99 and the register reflects the clamped value.
 *
 *  Rendering happens only when this module owns the screen: RUN mode with
 *  the OLED present. The combined light+display coils (1011-1018, 1031-1038)
 *  live in led_control (they are preset commands first) and drive this
 *  module through displayControlSetEnabled().
 */

/*  @brief Register the Modbus handlers (reg 60, coil 1010).
 *  @param ownScreen true when this module may draw: RUN mode + OLED present.
 *         When true the screen is cleared once here (removing the boot mode
 *         indicator) — display_control owns the RUN screen from boot on. */
void displayControlInit(bool ownScreen);

/*  @brief Turn the display on/off (renders reg 60 / clears the screen) and
 *         mirror coil 1010. Idempotent; safe to call from other handlers. */
void displayControlSetEnabled(bool on);

#endif // APP_DISPLAY_CONTROL_H
