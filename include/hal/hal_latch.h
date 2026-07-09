#ifndef HAL_LATCH_H
#define HAL_LATCH_H

#include <Arduino.h>
#include "system.h"

/*
 * hal_latch.h - Hardware abstraction for the electronic latch (drawer lock)
 * -------------------------------------------------------------------------
 * Wraps the MOSFET drive output and the latch sense input so the application
 * logic never touches MOSFET_PIN / SENSE_PIN directly.
 */

// Latch safety limits
#define LATCH_MAX_UNLOCK_TIME       500     // Maximum unlock time in ms (safety limit)
#define LATCH_MIN_INTERVAL          2000    // Minimum interval between unlocks in ms

/* @brief Configure the latch GPIOs (call once during startup). */
void latchInit();

/* @brief Check if the latch is currently locked (sense pin active LOW).
 * @param debounceDelay Optional debounce delay in ms.
 * @return true if locked, false otherwise. */
bool isLatchLocked(int debounceDelay = 0);

/* @brief Drive the latch open for a bounded amount of time.
 * @param unlockTimeout Requested unlock time in ms (clamped to LATCH_MAX_UNLOCK_TIME).
 * @return true if the latch was actuated, false if rejected/already unlocked. */
bool unlockLatch(int unlockTimeout = 300);

#endif // HAL_LATCH_H
