#ifndef SVC_STATS_H
#define SVC_STATS_H

#include <Arduino.h>

/*  @file svc/stats.h
 *  @brief Persistent module statistics: storage, counters and publication.
 *
 *  One blob on the AT24 (STATS_AT24_ADDR) holds every lifetime counter:
 *  boot count, per-preset LED on-count/on-time, latch (solenoid) firings,
 *  pick-confirm button presses, operating seconds and IWDG reset count.
 *  Written change-detected: hourly (led_control's tick), before every
 *  commanded reset (opsSystemReset) and once per boot (statsBootCommit).
 *  A power cut therefore loses at most the last un-flushed hour.
 *
 *  Publication covers BOTH register views: the legacy clamped u16 group
 *  (200-281, saturates at 65535) and the Statistics-v2 u32 block (400-451,
 *  hi word first — fw >= v3.3.0).
 *
 *  Layering: svc only (drivers + svc includes). app modules call in via
 *  the statsNote... and statsAdd... accessors; the one upward value (boot
 *  count to reg 7's diag setter) is routed by appInit, not from here.
 */

/*  @brief Load (and if needed migrate) the blob from the AT24. Never
 *         writes — not to the EEPROM, not to the Modbus registers (the
 *         server is not up yet when this runs). Call once, early. */
void statsInit();

/*  @brief Count this boot: bootCount+1, fold a pending IWDG note, persist
 *         in ONE EEPROM write. Call once per boot, after the reset cause
 *         has been reported via statsNoteResetCause().
 *  @return the new boot count (route it to diagSetBootCount) */
uint16_t statsBootCommit();

/*  @brief Report the reset cause bits (reg-8 encoding, bit0 = IWDG).
 *         Called by diagControlInit; folded at statsBootCommit(). */
void statsNoteResetCause(uint16_t causeBits);

/*  @brief One LED on-transition of preset n (1-8). */
void statsNoteLedOn(uint8_t preset);

/*  @brief Add whole seconds of on-time to preset n (1-8). The caller
 *         (led_control) keeps the sub-second remainder. */
void statsAddOnTime(uint8_t preset, uint32_t seconds);

/*  @brief One physical solenoid energization (latch_control's pulse start).
 *         Safety-refused requests never reach that point, so they are
 *         never counted; DEMO bench pulses are (wear odometer). */
void statsNoteLatchFire();

/*  @brief One accepted pick-confirm press (RUN mode, debounced). */
void statsNotePress();

/*  @brief Persist to the AT24 when anything changed. Folds the running
 *         operating-time into opSeconds first, so calling this at least
 *         hourly keeps the odometer honest. */
void statsPersistIfChanged();

/*  @brief Zero every usage counter (LED, latch, presses, op-time, IWDG)
 *         and persist. Boot count survives — the one "since first
 *         commissioning" number. Coil 510 and factory reset use this. */
void statsClearUsage();

/*  @brief Publish all statistics registers: legacy 200-281 (u16 clamped)
 *         and the v2 u32 block 400-451. Called every led_control tick. */
void statsPublishRegisters(uint32_t now);

#endif // SVC_STATS_H
