#ifndef APP_LATCH_CONTROL_H
#define APP_LATCH_CONTROL_H

#include <Arduino.h>
#include "config.h"

/*  @file app/latch_control.h
 *  @brief Electronic latch policy: unlock pulse and lock-state tracking.
 *
 *  NOTE: currently still the original BLOCKING implementation (moved
 *  verbatim from system.cpp). The non-blocking pulse state machine
 *  replaces it in a later, separately verified step.
 */

// Timestamp of the last time the latch was seen locked (used by the
// time-after-unlock Modbus register).
extern uint32_t lastTimeLatchLocked;

/*  @brief Check if the latch is locked.
 *  @param debounceDelay Optional blocking debounce in milliseconds
 *  @return true if the latch is locked
 */
bool isLatchLocked(int debounceDelay = 0);

/*  @brief Energize the latch MOSFET to unlock, bounded by safety limits.
 *
 *  Pulse width is clamped to LATCH_MAX_UNLOCK_TIME and requests closer
 *  than LATCH_MIN_INTERVAL to the previous unlock are rejected.
 *
 *  @param unlockTimeout Requested pulse width in milliseconds
 *  @return true if the pulse was performed
 */
bool unlockLatch(int unlockTimeout = LATCH_PULSE_MS);

#endif // APP_LATCH_CONTROL_H
