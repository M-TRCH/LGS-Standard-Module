#include "app/latch_control.h"
#include "drivers/board_io.h"
#include "svc/modbus_map.h"
#include "svc/modbus_server.h"

namespace {

uint32_t lastTimeLatchLocked = 0;

// Latch trigger coil (1020)
void onLatchTriggerCommand(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;
    delay(mbRegRead(MB_REG_UNLOCK_DELAY)); // Configured pre-unlock delay (ms)
    unlockLatch(LATCH_PULSE_MS);           // Safety limits enforced in function
    mbCoilWrite(MB_COIL_LATCH_TRIGGER, false);
}

} // namespace

void latchControlInit()
{
    mbRegisterHandler(MB_WATCH_COIL_COMMAND, MB_COIL_LATCH_TRIGGER, onLatchTriggerCommand);
}

void latchControlTick(uint32_t now)
{
#ifndef DISABLE_LATCH_STATUS_RESET
    if (isLatchLocked())
    {
        lastTimeLatchLocked = now;
        mbRegWrite(MB_REG_TIME_AFTER_UNLOCK, 0);
    }
    else
    {
        mbRegWrite(MB_REG_TIME_AFTER_UNLOCK, (uint16_t)((now - lastTimeLatchLocked) / 1000)); // seconds
    }
#else
    (void)now;
    (void)lastTimeLatchLocked;
    mbRegWrite(MB_REG_TIME_AFTER_UNLOCK, 0); // latch status reporting disabled
#endif
}

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
