#ifndef APP_LATCH_CONTROL_H
#define APP_LATCH_CONTROL_H

#include <Arduino.h>

/*  @file app/latch_control.h
 *  @brief Electronic latch policy: non-blocking unlock pulse state machine,
 *         lock-state tracking and the latch Modbus surface (coil 1020,
 *         reg 40).
 *
 *  Pulse state machine: IDLE -> DELAY -> PULSE -> COOLDOWN -> IDLE.
 *  Safety limits survive structurally:
 *    - pulse width is clamped to LATCH_MAX_UNLOCK_TIME and checked every tick
 *    - COOLDOWN spans LATCH_MIN_INTERVAL measured from PULSE START (matching
 *      the original lastUnlockTime reference)
 *    - outside PULSE the MOSFET is forced low on every tick, so a missed
 *      transition can never leave the gate energized
 */

/*  @brief Register the Modbus handler for the latch trigger coil. */
void latchControlInit();

/*  @brief Advance the pulse state machine, enforce the MOSFET-low invariant,
 *         track the debounced lock state and publish reg 40.
 *         Must run unconditionally every loop, before mode logic. */
void latchControlTick(uint32_t now);

/*  @brief Request an unlock pulse (accepted only when the machine is IDLE).
 *
 *  On acceptance the configured unlock delay (reg 80, clamped to
 *  UNLOCK_DELAY_MAX_MS) elapses first, then the MOSFET pulses until the
 *  sense pin releases or the clamped width expires. When the pulse
 *  completes, @p coilToClear is written back to 0 and, if requested, the
 *  LED enable coil is set — the same externally visible completion point
 *  as the original blocking implementation.
 *
 *  @param pulseMs        Requested pulse width (clamped to LATCH_MAX_UNLOCK_TIME)
 *  @param coilToClear    Modbus coil cleared when the request resolves
 *  @param syncEnableCoil Set the LED enable coil on completion (LED-latch command)
 *  @return true when accepted; false while DELAY/PULSE/COOLDOWN is active —
 *          the caller must clear its coil immediately to avoid a retry loop
 */
bool latchRequestUnlock(uint16_t pulseMs, uint16_t coilToClear, bool syncEnableCoil);

/*  @brief true while an accepted request for @p coil is still in flight
 *         (used by command handlers to keep their coil set without
 *         re-triggering). */
bool latchBusyWith(uint16_t coil);

#endif // APP_LATCH_CONTROL_H
