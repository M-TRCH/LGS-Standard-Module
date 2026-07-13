#include "system.h"
#include "hw/serial.h"
#include "hw/sensor.h"

// Global variables
uint32_t lastTimeRoutineBlink = 0;
uint32_t lastTimeRoutineSetID = 0;
uint32_t lastTimeSensorRead = 0;
bool blink_run_state = false;
bool blink_set_id_state = false;
FunctionSwitchMode functionMode = FUNC_SW_RUN;
uint32_t lastTimeLatchLocked = 0;

void sysInit()
{
    serialInit();

    // Initialize system-level GPIO
    pinMode(HW_LED_BUILTIN_PIN, OUTPUT);
    pinMode(HW_LATCH_TRIGGER_PIN, OUTPUT);
    pinMode(HW_LATCH_CHECK_PIN, INPUT_PULLUP);
    pinMode(HW_FUNCTION_SWITCH_PIN, INPUT);

    sysSetRunIndicator(false);
    digitalWrite(HW_LATCH_TRIGGER_PIN, LOW);

    // Initialize the internal temperature sensor (I2C1)
    sensorInit();

    // Check function switch immediately after system init
    functionMode = checkFunctionSwitch();
}

void sysSetRunIndicator(bool state)
{
    digitalWrite(HW_LED_BUILTIN_PIN, state ? HIGH : LOW);
}

bool sysIsFunctionSwitchPressed()
{
    return (digitalRead(HW_FUNCTION_SWITCH_PIN) == LOW);
}

bool isLatchLocked(int debounceDelay)
{
    if (digitalRead(HW_LATCH_CHECK_PIN) == LOW)
    {
        delay(debounceDelay); // Debounce delay
        if (digitalRead(HW_LATCH_CHECK_PIN) == LOW)
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

    if (digitalRead(HW_LATCH_CHECK_PIN) == LOW)
    {
        lastUnlockTime = millis();
        digitalWrite(HW_LATCH_TRIGGER_PIN, HIGH);  // Activate MOSFET to unlock latch

        uint32_t startTime = millis();
        while (digitalRead(HW_LATCH_CHECK_PIN) == LOW)
        {
            if (millis() - startTime >= unlockTimeout)
            {
                break; // Exit if timeout reached
            }
        }

        digitalWrite(HW_LATCH_TRIGGER_PIN, LOW);  // Deactivate MOSFET after timeout

        return true; // Latch is active
    }
    else
    {
        return false; // Latch is inactive
    }
}

FunctionSwitchMode checkFunctionSwitch(uint16_t maxWaitTime)
{
    // Check if switch is pressed at startup (active LOW)
    if (digitalRead(HW_FUNCTION_SWITCH_PIN) == HIGH)
    {
        return FUNC_SW_RUN;  // Switch not pressed, continue normal operation
    }

    // Switch is pressed, measure how long it's held
    uint32_t pressStartTime = millis();
    uint32_t pressDuration = 0;
    uint32_t lastBlinkCycle = 0;

    // Wait for switch release or max time
    while (digitalRead(HW_FUNCTION_SWITCH_PIN) == LOW && (millis() - pressStartTime) < maxWaitTime)
    {
        pressDuration = millis() - pressStartTime;
        uint32_t currentCycle = pressDuration / 1000;  // Each cycle is 1 second

        // Check if we entered a new cycle
        if (currentCycle > lastBlinkCycle)
        {
            lastBlinkCycle = currentCycle;
        }

        // Determine blink pattern based on press duration
        uint8_t blinksPerCycle = 0;  // Default: No blink (0-2s)
        if (pressDuration >= 8000)
        {
            blinksPerCycle = 4;  // FACTORY_RESET: 4 blinks per cycle (8-11s)
        }
        else if (pressDuration >= 5000)
        {
            blinksPerCycle = 2;  // SET_ID: 2 blinks per cycle (5-8s)
        }
        else if (pressDuration >= 2000)
        {
            blinksPerCycle = 1;  // DEMO: 1 blink per cycle (2-5s)
        }
        else
        {
            blinksPerCycle = 0;  // No action: 0 blinks (0-2s) - prevent accidental press
        }

        // Calculate position within current cycle (0-999ms)
        uint32_t cyclePosition = pressDuration % 1000;

        // Blink pattern timing (each blink: 150ms ON, 100ms OFF)
        uint32_t blinkDuration = 150;   // LED ON time
        uint32_t blinkPause = 100;      // LED OFF time between blinks
        uint32_t singleBlinkTime = blinkDuration + blinkPause;  // 250ms per blink

        bool shouldLedBeOn = false;
        for (uint8_t i = 0; i < blinksPerCycle; i++)
        {
            uint32_t blinkStart = i * singleBlinkTime;
            uint32_t blinkEnd = blinkStart + blinkDuration;

            if (cyclePosition >= blinkStart && cyclePosition < blinkEnd)
            {
                shouldLedBeOn = true;
                break;
            }
        }

        sysSetRunIndicator(shouldLedBeOn);

        delay(50);  // Small delay to reduce CPU usage
    }

    // Turn off LED after release
    sysSetRunIndicator(false);

    // Determine which mode based on press duration
    FunctionSwitchMode mode = FUNC_SW_RUN;

    if (pressDuration >= 8000 && pressDuration < 11000)  // 8-11 seconds
    {
        mode = FUNC_SW_FACTORY_RESET;
    }
    else if (pressDuration >= 5000 && pressDuration < 8000)  // 5-8 seconds
    {
        mode = FUNC_SW_SET_ID;
    }
    else if (pressDuration >= 2000 && pressDuration < 5000)  // 2-5 seconds
    {
        mode = FUNC_SW_DEMO;
    }
    else
    {
        // Less than 2 seconds or more than 11 seconds - no action
        mode = FUNC_SW_RUN;
    }

    // Wait a bit to ensure switch is fully released
    delay(100);

    return mode;
}

bool ON_ROUTINE_BLINK_RUN()
{
    uint32_t currentMillis = millis();
    if (currentMillis - lastTimeRoutineBlink >= ROUTINE_BLINK_RUN_MS)
    {
        lastTimeRoutineBlink = currentMillis;
        blink_run_state = !blink_run_state;
        return true;
    }
    return false;
}

bool ON_ROUTINE_BLINK_SET_ID()
{
    uint32_t currentMillis = millis();
    if (currentMillis - lastTimeRoutineSetID >= ROUTINE_BLINK_SET_ID_MS)
    {
        lastTimeRoutineSetID = currentMillis;
        blink_set_id_state = !blink_set_id_state;
        return true;
    }
    return false;
}

bool ON_ROUTINE_SENSOR_READ()
{
    uint32_t currentMillis = millis();
    if (currentMillis - lastTimeSensorRead >= ROUTINE_SENSOR_READ_MS)
    {
        lastTimeSensorRead = currentMillis;
        return true;
    }
    return false;
}
