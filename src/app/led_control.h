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

/*  @brief true while the channel is logically on (used by latch_control to
 *         decide whether completing an LED-latch request may sync the
 *         enable coil). */
bool ledControlChannelOn();

/*  @brief Enable-coil address (1000+n) of the currently active preset, or 0
 *         when the ring is off. latch_control compares this against a
 *         combo's requested coil before syncing it on pulse completion. */
uint16_t ledControlActiveEnableCoil();

#endif // APP_LED_CONTROL_H
