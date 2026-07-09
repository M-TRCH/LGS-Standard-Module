#include "hal/hal_latch.h"

void latchInit()
{
    pinMode(MOSFET_PIN, OUTPUT);
    pinMode(SENSE_PIN, INPUT_PULLUP);
    digitalWrite(MOSFET_PIN, LOW);
    LOG_DEBUG_SYS(F("[LATCH] Latch HAL initialized\n"));
}

bool isLatchLocked(int debounceDelay)
{
    if (digitalRead(SENSE_PIN) == LOW)
    {
        delay(debounceDelay); // Debounce delay
        if (digitalRead(SENSE_PIN) == LOW)
        {
            return true; // Latch is locked
        }
    }
    return false; // Latch is inactive
}

bool unlockLatch(int unlockTimeout)
{
    // Safety check: enforce maximum unlock time
    if (unlockTimeout > LATCH_MAX_UNLOCK_TIME)
    {
        LOG_WARNING_SYS("[LATCH] Unlock timeout " + String(unlockTimeout) + "ms exceeds maximum " + String(LATCH_MAX_UNLOCK_TIME) + "ms, clamping to max\n");
        unlockTimeout = LATCH_MAX_UNLOCK_TIME;
    }

    // Safety check: prevent frequent unlocking
    static uint32_t lastUnlockTime = 0;
    uint32_t timeSinceLastUnlock = millis() - lastUnlockTime;

    if (lastUnlockTime != 0 && timeSinceLastUnlock < LATCH_MIN_INTERVAL)
    {
        LOG_WARNING_SYS("[LATCH] Unlock attempt blocked - only " + String(timeSinceLastUnlock) + "ms since last unlock (min " + String(LATCH_MIN_INTERVAL) + "ms)\n");
        return false; // Reject unlock attempt if too soon
    }

    if (digitalRead(SENSE_PIN) == LOW)
    {
        lastUnlockTime = millis();
        digitalWrite(MOSFET_PIN, HIGH);  // Activate MOSFET to unlock latch
        LOG_INFO_SYS("[LATCH] Latch unlocking for " + String(unlockTimeout) + "ms\n");

        uint32_t startTime = millis();
        while (digitalRead(SENSE_PIN) == LOW)
        {
            if (millis() - startTime >= (uint32_t)unlockTimeout)
            {
                break; // Exit if timeout reached
            }
        }

        uint32_t actualDuration = millis() - startTime;
        digitalWrite(MOSFET_PIN, LOW);  // Deactivate MOSFET after timeout
        LOG_INFO_SYS("[LATCH] Latch unlocked for " + String(actualDuration) + "ms\n");

        return true; // Latch was actuated
    }
    else
    {
        LOG_DEBUG_SYS("[LATCH] Unlock attempt - latch already inactive\n");
        return false; // Latch is inactive
    }
}
