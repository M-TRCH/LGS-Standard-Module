#ifndef APP_MODES_H
#define APP_MODES_H

#include <Arduino.h>

/*  @file app/modes.h
 *  @brief Operating-mode selection via the boot-time function switch.
 */

// Function switch modes (returned by checkFunctionSwitch)
enum FunctionSwitchMode
{
    FUNC_SW_RUN = 0,            // No switch pressed (normal operation)
    FUNC_SW_DEMO = 1,           // Pressed 2-5 seconds
    FUNC_SW_SET_ID = 2,         // Pressed 5-8 seconds
    FUNC_SW_FACTORY_RESET = 3   // Pressed 8-11 seconds
};

/*  @brief Measure the boot-time function switch hold and classify the mode.
 *
 *  Deliberately BLOCKING: runs once in appInit() before the Modbus server
 *  starts, and only blocks while a human physically holds the button.
 *  Blink feedback: 1 blink/cycle = DEMO, 2 = SET_ID, 4 = FACTORY_RESET.
 *
 *  @param maxWaitTime Maximum time to wait for switch release (ms)
 */
FunctionSwitchMode checkFunctionSwitch(uint16_t maxWaitTime = 15000);

#endif // APP_MODES_H
