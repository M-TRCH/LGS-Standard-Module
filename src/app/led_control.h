#ifndef APP_LED_CONTROL_H
#define APP_LED_CONTROL_H

#include <Arduino.h>

/*  @file app/led_control.h
 *  @brief LED policy behind coils 1001-1038: eight color presets with radio
 *         switching on ring boards, eight INDEPENDENT windows on mask
 *         boards (type 10, v3.5.0 — window n = person n, several lit at
 *         once). One engine or the other is chosen at init from the
 *         commissioned device type; the coil family, per-preset config and
 *         max-on-time registers are shared.
 *
 *  Owns the LED runtime state (active preset / lit-window set, in-flight
 *  on-intervals). Counter storage and register publication live in
 *  svc/stats; pixel work is delegated to drivers/led_ring and led_mask.
 */

/*  @brief Register the Modbus handlers (enable coils 1001-1008, LED-latch
 *         coils 1021-1028, global brightness / max-on-time fan-outs). */
void ledControlInit();

/*  @brief Enforce max-on-time and publish statistics registers.
 *         Runs unconditionally every loop, before mode logic. */
void ledControlTick(uint32_t now);

/*  @brief One frame of the DEMO rainbow, on whichever display is fitted.
 *
 *  DEMO used to call the ring driver directly, which meant a Standard
 *  cabinet — mask, no ring — showed nothing at all in the one mode built
 *  for looking at a board. Routing it through here keeps "which display
 *  does this board have" answered in exactly one place, the same place
 *  the preset engine asks. */
void ledControlShowDemoFrame(uint16_t phase);

/*  @brief true while the channel is logically on (used by latch_control to
 *         decide whether completing an LED-latch request may sync the
 *         enable coil). */
bool ledControlChannelOn();

/*  @brief true when @p coil (1001-1008) belongs to a preset/window that is
 *         lit right now. latch_control checks this before syncing a combo's
 *         enable coil on pulse completion — on ring boards it is exactly
 *         the old "is this the active preset's coil" comparison; on mask
 *         boards any lit window qualifies. */
bool ledControlEnableCoilOn(uint16_t coil);

/*  @brief Active preset number (0 = off, 1-8), published at reg 11. On mask
 *         boards: the last window commanded on while it is still lit, else
 *         the lowest lit — single-window use reads exactly like the ring;
 *         the full multi-window truth is ledControlLitWindows(). */
uint8_t ledControlActivePreset();

/*  @brief Bitmask of lit windows (bit n-1 = preset/window n), published at
 *         reg 61. On ring boards: the active preset's bit, or 0. */
uint8_t ledControlLitWindows();

/*  @brief Flush the statistics to the AT24 if they changed (folds the
 *         running on-interval in first). Called hourly from the tick and
 *         by opsSystemReset before every commanded reboot. */
void ledControlPersistStats();

/*  @brief Zero all statistics (wire + persistent blob). Coil 510 and
 *         factory reset use this. */
void ledControlClearStats();

/*  @brief Short white ring-blink acknowledging a pick-confirm button press.
 *         Same non-destructive overlay as identify (coil 509), shorter
 *         window; the ring returns to the active preset afterwards. */
void ledControlConfirmBlink();

#endif // APP_LED_CONTROL_H
