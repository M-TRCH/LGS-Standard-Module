#ifndef APP_DISPLAY_CONTROL_H
#define APP_DISPLAY_CONTROL_H

#include <Arduino.h>

/*  @file app/display_control.h
 *  @brief OLED display policy driven by Modbus: reg 60 (number) + coil 1010
 *         (enable). While enabled, the number renders in the big digit
 *         font (the DEMO face); fw >= v3.4.0 renders 0-999 (0-99 before) —
 *         out-of-range writes to reg 60 clamp to 999 and the register
 *         reflects the clamped value. Below 100 the familiar two-digit
 *         face is kept ("45", not "045").
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

/*  @brief Firmware-driven number update: write reg 60 and re-render if the
 *         display is enabled (bus writes go through the reg-60 handler; this
 *         is for internal producers). */
void displayControlShowNumber(uint16_t value);

/*  @brief Take the screen over with the firmware-update face (caption +
 *         percent + chunk count). Does NOT touch reg 60 or coil 1010: an
 *         update in progress is not a slot lit for a pick, and the two used
 *         to be indistinguishable both on the glass and on the wire.
 *         displayControlSetEnabled(false) hands the screen back. */
void displayControlShowOta(uint8_t percent, uint16_t done, uint16_t total);

#endif // APP_DISPLAY_CONTROL_H
