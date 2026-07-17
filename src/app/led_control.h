#ifndef APP_LED_CONTROL_H
#define APP_LED_CONTROL_H

#include <Arduino.h>

/*  @file app/led_control.h
 *  @brief LED preset policy: eight color presets on the single ring with
 *         radio switching, Modbus enable/latch coil families, per-preset
 *         config application, max-on-time enforcement and statistics.
 *
 *  Owns all LED runtime state (active preset, counters, on-time
 *  accumulators). The pixel-level work is delegated to drivers/led_ring.
 */

/*  @brief Register the Modbus handlers (enable coils 1001-1008, LED-latch
 *         coils 1021-1028, global brightness / max-on-time fan-outs). */
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

/*  @brief Active preset number (0 = ring off, 1-8). Published at reg 11. */
uint8_t ledControlActivePreset();

#endif // APP_LED_CONTROL_H
