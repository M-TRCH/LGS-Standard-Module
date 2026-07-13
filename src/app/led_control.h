#ifndef APP_LED_CONTROL_H
#define APP_LED_CONTROL_H

#include <Arduino.h>

/*  @file app/led_control.h
 *  @brief LED channel policy: Modbus enable/latch coils, configured color
 *         application, max-on-time enforcement and usage statistics.
 *
 *  Owns all LED runtime state (on/off, counters, on-time accumulators).
 *  The pixel-level work is delegated to drivers/led_ring.
 */

/*  @brief Register the Modbus handlers (enable coil, LED-latch coil,
 *         global brightness / max-on-time fan-outs). */
void ledControlInit();

/*  @brief Enforce max-on-time and publish statistics registers.
 *         Runs unconditionally every loop, before mode logic. */
void ledControlTick(uint32_t now);

#endif // APP_LED_CONTROL_H
