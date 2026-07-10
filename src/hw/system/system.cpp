#include "system.h"
#include "hw/serial.h"
#include "hw/sensor.h"

// Global variables
uint32_t lastTimeRoutineBlink = 0;
uint32_t lastTimeRoutineDemo = 0;
uint32_t lastTimeRoutineSetID = 0;
uint32_t lastTimeSensorRead = 0;
bool blink_run_state = false;
bool blink_demo_state = false;
bool blink_set_id_state = false;
FunctionSwitchMode functionMode = FUNC_SW_RUN;
uint32_t lastTimeLatchLocked = 0;

// Log configuration
LogLevel globalLogLevel = LOG_INFO;
uint8_t enabledLogCategories = LOG_CAT_ALL;

void sysInit(LogLevel logLevel, uint8_t logCategories)
{
    globalLogLevel = logLevel;
    enabledLogCategories = logCategories;

    // Bring up the UARTs first so logging is visible immediately
    serialInit();

    LOG_INFO_SYS(F("\n[SYSTEM] Initializing system...\n"));

    // Initialize system-level GPIO
    pinMode(LED_RUN_PIN, OUTPUT);
    pinMode(MOSFET_PIN, OUTPUT);
    pinMode(SENSE_PIN, INPUT_PULLUP);
    pinMode(FUNC_SW_PIN, INPUT);

    sysSetRunIndicator(false);
    digitalWrite(MOSFET_PIN, LOW);

    // Initialize the internal temperature sensor (I2C1)
    sensorInit();

    // Check function switch immediately after system init
    functionMode = checkFunctionSwitch();

    LOG_INFO_SYS(F("[SYSTEM] Initialization complete\n"));
}

void sysSetRunIndicator(bool state)
{
    digitalWrite(LED_RUN_PIN, state ? HIGH : LOW);
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
        LOG_WARNING_SYS("[SYSTEM] Unlock timeout " + String(unlockTimeout) + "ms exceeds maximum " + String(LATCH_MAX_UNLOCK_TIME) + "ms, clamping to max\n");
        unlockTimeout = LATCH_MAX_UNLOCK_TIME;
    }

    // Safety check: prevent frequent unlocking
    static uint32_t lastUnlockTime = 0;
    uint32_t timeSinceLastUnlock = millis() - lastUnlockTime;

    if (lastUnlockTime != 0 && timeSinceLastUnlock < LATCH_MIN_INTERVAL)
    {
        LOG_WARNING_SYS("[SYSTEM] Unlock attempt blocked - only " + String(timeSinceLastUnlock) + "ms since last unlock (min " + String(LATCH_MIN_INTERVAL) + "ms)\n");
        return false; // Reject unlock attempt if too soon
    }

    if (digitalRead(SENSE_PIN) == LOW)
    {
        lastUnlockTime = millis();
        digitalWrite(MOSFET_PIN, HIGH);  // Activate MOSFET to unlock latch
        LOG_INFO_SYS("[SYSTEM] Latch unlocking for " + String(unlockTimeout) + "ms\n");

        uint32_t startTime = millis();
        while (digitalRead(SENSE_PIN) == LOW)
        {
            if (millis() - startTime >= unlockTimeout)
            {
                break; // Exit if timeout reached
            }
        }

        uint32_t actualDuration = millis() - startTime;
        digitalWrite(MOSFET_PIN, LOW);  // Deactivate MOSFET after timeout
        LOG_INFO_SYS("[SYSTEM] Latch unlocked for " + String(actualDuration) + "ms\n");

        return true; // Latch is active
    }
    else
    {
        LOG_DEBUG_SYS("[SYSTEM] Unlock attempt - latch already inactive\n");
        return false; // Latch is inactive
    }
}

FunctionSwitchMode checkFunctionSwitch(uint16_t maxWaitTime)
{
    LOG_DEBUG_SYS(F("[SYSTEM] Checking function switch...\n"));

    // Check if switch is pressed at startup (active LOW)
    if (digitalRead(FUNC_SW_PIN) == HIGH)
    {
        LOG_DEBUG_SYS(F("[SYSTEM] Function switch not pressed, continuing normal operation\n"));
        return FUNC_SW_RUN;  // Switch not pressed, continue normal operation
    }

    LOG_INFO_SYS(F("[SYSTEM] Function switch detected! Waiting for release...\n"));

    // Switch is pressed, measure how long it's held
    uint32_t pressStartTime = millis();
    uint32_t pressDuration = 0;
    uint32_t lastBlinkCycle = 0;

    // Wait for switch release or max time
    while (digitalRead(FUNC_SW_PIN) == LOW && (millis() - pressStartTime) < maxWaitTime)
    {
        pressDuration = millis() - pressStartTime;
        uint32_t currentCycle = pressDuration / 1000;  // Each cycle is 1 second

        // Check if we entered a new cycle
        if (currentCycle > lastBlinkCycle)
        {
            lastBlinkCycle = currentCycle;
            LOG_INFO_SYS("[SYSTEM] Switch pressed: " + String(currentCycle) + " seconds...\n");
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
        LOG_INFO_SYS(F("[SYSTEM] Function switch: FACTORY_RESET (8-11s) detected\n"));
    }
    else if (pressDuration >= 5000 && pressDuration < 8000)  // 5-8 seconds
    {
        mode = FUNC_SW_SET_ID;
        LOG_INFO_SYS(F("[SYSTEM] Function switch: SET_ID (5-8s) detected\n"));
    }
    else if (pressDuration >= 2000 && pressDuration < 5000)  // 2-5 seconds
    {
        mode = FUNC_SW_DEMO;
        LOG_INFO_SYS(F("[SYSTEM] Function switch: DEMO (2-5s) detected\n"));
    }
    else
    {
        // Less than 2 seconds or more than 11 seconds - no action
        mode = FUNC_SW_RUN;
        LOG_INFO_SYS("[SYSTEM] Function switch: No action (press duration: " + String(pressDuration) + "ms)\n");
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

bool ON_ROUTINE_BLINK_DEMO()
{
    uint32_t currentMillis = millis();
    if (currentMillis - lastTimeRoutineDemo >= ROUTINE_BLINK_DEMO_MS)
    {
        lastTimeRoutineDemo = currentMillis;
        blink_demo_state = !blink_demo_state;
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
