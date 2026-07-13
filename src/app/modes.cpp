#include "app/modes.h"
#include "drivers/board_io.h"

FunctionSwitchMode checkFunctionSwitch(uint16_t maxWaitTime)
{
    // Check if switch is pressed at startup (active LOW)
    if (!boardFunctionSwitchPressed())
    {
        return FUNC_SW_RUN;  // Switch not pressed, continue normal operation
    }

    // Switch is pressed, measure how long it's held
    uint32_t pressStartTime = millis();
    uint32_t pressDuration = 0;
    uint32_t lastBlinkCycle = 0;

    // Wait for switch release or max time
    while (boardFunctionSwitchPressed() && (millis() - pressStartTime) < maxWaitTime)
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

        boardSetRunLed(shouldLedBeOn);

        delay(50);  // Small delay to reduce CPU usage
    }

    // Turn off LED after release
    boardSetRunLed(false);

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
