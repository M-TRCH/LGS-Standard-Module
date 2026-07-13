
#ifndef SYSTEM_H
#define SYSTEM_H

#include <Arduino.h>
#include "config.h"
#include "HW_config.h"

// Hardware pin mapping is centralized in HW_config.h
// Hardware drivers are split under include/hw and src/hw

// Communication settings
#define MODBUS_BAUD     9600

// System settings
#define ROUTINE_BLINK_RUN_MS        1200    // LED blink interval in normal operation
#define ROUTINE_BLINK_DEMO_MS       800     // LED blink interval in demo mode
#define ROUTINE_BLINK_SET_ID_MS     800     // LED blink interval in set ID mode
#define ROUTINE_SENSOR_READ_MS      1000    // Sensor read interval

// Latch safety settings
#define LATCH_MAX_UNLOCK_TIME       500     // Maximum unlock time in milliseconds (safety limit)
#define LATCH_MIN_INTERVAL          2000    // Minimum interval between unlock calls in milliseconds

// Function switch modes (returned by checkFunctionSwitch)
enum FunctionSwitchMode
{
    FUNC_SW_RUN = 0,            // No switch pressed (normal operation)
    FUNC_SW_DEMO = 1,           // Pressed > 1 second
    FUNC_SW_SET_ID = 2,         // Pressed > 5 seconds
    FUNC_SW_FACTORY_RESET = 3   // Pressed > 10 seconds
};

// Global variables
extern uint32_t lastTimeRoutineBlink;
extern uint32_t lastTimeRoutineDemo;
extern uint32_t lastTimeRoutineSetID;
extern uint32_t lastTimeSensorRead;
extern bool blink_run_state;
extern bool blink_demo_state;
extern bool blink_set_id_state;
extern FunctionSwitchMode functionMode;
extern uint32_t lastTimeLatchLocked;

/* @brief Initialize the system
 *
 * This function sets up the necessary pins and initializes the RS485 UART.
*/
void sysInit();

/* @brief Control the built-in status LED.
 *
 * @param state true to turn on, false to turn off
 */
void sysSetRunIndicator(bool state);

/* @brief Read the runtime state of the function switch.
 *
 * The switch is active LOW.
 *
 * @return true when the switch is currently pressed
 */
bool sysIsFunctionSwitchPressed();

/* @brief Check if the latch is locked
 *
 * This function reads the state of the latch and returns true if it is locked, false otherwise.
 *
 * @return true if the latch is locked, false otherwise
 */
bool isLatchLocked(int debounceDelay = 0);

/* @brief Check if the latch is active
 *
 * This function checks if the latch has been active within the specified timeout period.
 *
 * @param activeTimeout The timeout period in milliseconds (default is 300 ms)
 * @return true if the latch is active, false otherwise
 */
bool unlockLatch(int unlockTimeout = 300);

/* @brief Check function switch state at startup
 *
 * This function checks if the function switch (FUNC_SW_PIN) is pressed during startup.
 * It determines the press duration and returns the corresponding mode.
 * The switch is active LOW (pressed = LOW).
 *
 * @param maxWaitTime Maximum time to wait for switch press in milliseconds (default 15000ms)
 * @return FunctionSwitchMode:
 *         - FUNC_SW_RUN (0): Switch not pressed, continue normal operation
 *         - FUNC_SW_DEMO (1): Pressed 0-3 seconds
 *         - FUNC_SW_SET_ID (2): Pressed 4-8 seconds
 *         - FUNC_SW_FACTORY_RESET (3): Pressed 8-12 seconds
 */
FunctionSwitchMode checkFunctionSwitch(uint16_t maxWaitTime = 15000);

/* @brief Check if it's time for routine blink
 *
 * This function checks if the routine blink interval has elapsed.
 *
 * @return true if it's time to blink, false otherwise
 */
bool ON_ROUTINE_BLINK_RUN();

/* @brief Check if it's time for routine demo blink
 *
 * This function checks if the routine demo blink interval has elapsed.
 *
 * @return true if it's time to blink, false otherwise
 */
bool ON_ROUTINE_BLINK_DEMO();

/* @brief Check if it's time for routine set ID
 *
 * This function checks if the routine set ID interval has elapsed.
 *
 * @return true if it's time to set ID, false otherwise
 */
bool ON_ROUTINE_BLINK_SET_ID();

/* @brief Check if it's time for routine sensor read
 *
 * This function checks if the routine sensor read interval has elapsed.
 *
 * @return true if it's time to read the sensor, false otherwise
 */
bool ON_ROUTINE_SENSOR_READ();
#endif