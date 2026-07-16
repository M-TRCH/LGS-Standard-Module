#ifndef APP_LATCH_CONTROL_H
#define APP_LATCH_CONTROL_H

#include <Arduino.h>

/*  @file app/latch_control.h
 *  @brief Electronic latch policy: non-blocking unlock pulse state machine,
 *         lock-state tracking and the latch Modbus surface (coil 1019 force
 *         trigger, coil 1020 safety trigger, reg 40).
 *
 *  Pulse state machine: IDLE -> DELAY -> PULSE -> COOLDOWN -> IDLE.
 *  Safety limits survive structurally:
 *    - pulse width is clamped to LATCH_MAX_UNLOCK_TIME and checked every tick
 *    - COOLDOWN spans LATCH_MIN_INTERVAL measured from PULSE START (matching
 *      the original lastUnlockTime reference)
 *    - outside PULSE the MOSFET is forced low on every tick, so a missed
 *      transition can never leave the gate energized
 */

/*  @brief Register the Modbus handlers for the latch trigger coils:
 *         1020 = safety trigger (sense-aware), 1019 = force trigger
 *         (ignore sense, fixed full-width pulse). */
void latchControlInit();

/*  @brief Advance the pulse state machine, enforce the MOSFET-low invariant,
 *         track the debounced lock state and publish reg 40.
 *         Must run unconditionally every loop, before mode logic. */
void latchControlTick(uint32_t now);

/*  @brief Request an unlock pulse (accepted only when the machine is IDLE).
 *
 *  On acceptance the configured unlock delay (reg 80, clamped to
 *  UNLOCK_DELAY_MAX_MS) elapses first, then the MOSFET pulses.
 *
 *  Normal (sense-aware) mode: pulse only starts if the latch reads locked,
 *  energizes for at least @p pulseMs, then keeps energizing while still
 *  locked until the latch opens or the LATCH_MAX_UNLOCK_TIME cap.
 *
 *  @p ignoreSense mode (force trigger / bench test): the sense pin is not
 *  consulted at all — the pulse always starts and runs for a fixed
 *  @p pulseMs (pass LATCH_MAX_UNLOCK_TIME for the full 500ms).
 *
 *  When the pulse completes, @p coilToClear is written back to 0 and, if
 *  @p enableCoilToSync is nonzero, that preset-enable coil is set — but only
 *  when it still names the ACTIVE preset (a preset switch while the pulse is
 *  in flight must not resurrect a stale coil).
 *
 *  @param pulseMs          Minimum (or, with ignoreSense, exact) pulse width;
 *                          clamped to LATCH_MAX_UNLOCK_TIME
 *  @param coilToClear      Modbus coil cleared when the request resolves (0 = none)
 *  @param enableCoilToSync Preset-enable coil to set on completion (0 = none)
 *  @param ignoreSense      Skip all sense checks: always pulse, fixed pulseMs
 *  @return true when accepted; false while DELAY/PULSE/COOLDOWN is active —
 *          the caller must clear its coil immediately to avoid a retry loop
 */
bool latchRequestUnlock(uint16_t pulseMs, uint16_t coilToClear, uint16_t enableCoilToSync,
                        bool ignoreSense = false);

/*  @brief true while an accepted request for @p coil is still in flight
 *         (used by command handlers to keep their coil set without
 *         re-triggering). */
bool latchBusyWith(uint16_t coil);

#endif // APP_LATCH_CONTROL_H
