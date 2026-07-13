#include "app/latch_control.h"
#include "drivers/board_io.h"

uint32_t lastTimeLatchLocked = 0;

bool isLatchLocked(int debounceDelay)
{
    if (boardLatchSenseLow())
    {
        delay(debounceDelay); // Debounce delay
        if (boardLatchSenseLow())
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
        unlockTimeout = LATCH_MAX_UNLOCK_TIME;
    }

    // Safety check: prevent frequent unlocking
    static uint32_t lastUnlockTime = 0;
    uint32_t timeSinceLastUnlock = millis() - lastUnlockTime;

    if (lastUnlockTime != 0 && timeSinceLastUnlock < LATCH_MIN_INTERVAL)
    {
        return false; // Reject unlock attempt if too soon
    }

    if (boardLatchSenseLow())
    {
        lastUnlockTime = millis();
        boardLatchMosfetSet(true);  // Activate MOSFET to unlock latch

        uint32_t startTime = millis();
        while (boardLatchSenseLow())
        {
            if (millis() - startTime >= (uint32_t)unlockTimeout)
            {
                break; // Exit if timeout reached
            }
        }

        boardLatchMosfetSet(false);  // Deactivate MOSFET after timeout

        return true; // Latch is active
    }
    else
    {
        return false; // Latch is inactive
    }
}
