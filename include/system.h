
#ifndef SYSTEM_H
#define SYSTEM_H

#include <Arduino.h>
#include <Stream.h>
#include <HardwareSerial.h>
#include <ArduinoRS485.h>
#include "config.h"
#include "hw_config.h"   // Board-specific pin mapping

// Communication settings
#define DEBUG_BAUD      9600
#define MODBUS_BAUD     9600

// System settings
#define ROUTINE_BLINK_RUN_MS        1200    // LED blink interval in normal operation
#define ROUTINE_BLINK_DEMO_MS       800     // LED blink interval in demo mode
#define ROUTINE_BLINK_SET_ID_MS     800     // LED blink interval in set ID mode
#define ROUTINE_SENSOR_READ_MS      1000    // Sensor read interval

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

// Object declarations
extern HardwareSerial Serial3; 
extern RS485Class rs485;
extern RS485Class rs4853;

// Constants definitions
// Log level definitions
enum LogLevel
{
    LOG_NONE = 0,       // No logging
    LOG_ERROR,          // Critical errors only
    LOG_WARNING,        // Warnings and errors
    LOG_INFO,           // Informational messages, warnings, and errors
    LOG_DEBUG,          // Debug information
    LOG_VERBOSE         // Verbose debug information
};

// Log category definitions (bit flags)
enum LogCategory
{
    LOG_CAT_NONE    = 0x00,
    LOG_CAT_SYSTEM  = 0x01,     // System initialization and general operations
    LOG_CAT_EEPROM  = 0x02,     // EEPROM read/write operations
    LOG_CAT_MODBUS  = 0x04,     // Modbus communication
    LOG_CAT_LED     = 0x08,     // LED control operations
    LOG_CAT_ALL     = 0xFF      // All categories
};

// Global log configuration
extern LogLevel globalLogLevel;
extern uint8_t enabledLogCategories;

// Logging macros
#define LOG(level, category, msg) \
    do { \
        if ((globalLogLevel >= level) && (enabledLogCategories & category)) { \
            Serial.print(msg); \
        } \
    } while(0)

#define LOG_ERROR_SYS(msg)      LOG(LOG_ERROR, LOG_CAT_SYSTEM, msg)
#define LOG_WARNING_SYS(msg)    LOG(LOG_WARNING, LOG_CAT_SYSTEM, msg)
#define LOG_INFO_SYS(msg)       LOG(LOG_INFO, LOG_CAT_SYSTEM, msg)
#define LOG_DEBUG_SYS(msg)      LOG(LOG_DEBUG, LOG_CAT_SYSTEM, msg)
#define LOG_VERBOSE_SYS(msg)    LOG(LOG_VERBOSE, LOG_CAT_SYSTEM, msg)

#define LOG_ERROR_EEPROM(msg)   LOG(LOG_ERROR, LOG_CAT_EEPROM, msg)
#define LOG_WARNING_EEPROM(msg) LOG(LOG_WARNING, LOG_CAT_EEPROM, msg)
#define LOG_INFO_EEPROM(msg)    LOG(LOG_INFO, LOG_CAT_EEPROM, msg)
#define LOG_DEBUG_EEPROM(msg)   LOG(LOG_DEBUG, LOG_CAT_EEPROM, msg)
#define LOG_VERBOSE_EEPROM(msg) LOG(LOG_VERBOSE, LOG_CAT_EEPROM, msg)

#define LOG_ERROR_MODBUS(msg)   LOG(LOG_ERROR, LOG_CAT_MODBUS, msg)
#define LOG_WARNING_MODBUS(msg) LOG(LOG_WARNING, LOG_CAT_MODBUS, msg)
#define LOG_INFO_MODBUS(msg)    LOG(LOG_INFO, LOG_CAT_MODBUS, msg)
#define LOG_DEBUG_MODBUS(msg)   LOG(LOG_DEBUG, LOG_CAT_MODBUS, msg)
#define LOG_VERBOSE_MODBUS(msg) LOG(LOG_VERBOSE, LOG_CAT_MODBUS, msg)

#define LOG_ERROR_LED(msg)      LOG(LOG_ERROR, LOG_CAT_LED, msg)
#define LOG_WARNING_LED(msg)    LOG(LOG_WARNING, LOG_CAT_LED, msg)
#define LOG_INFO_LED(msg)       LOG(LOG_INFO, LOG_CAT_LED, msg)
#define LOG_DEBUG_LED(msg)      LOG(LOG_DEBUG, LOG_CAT_LED, msg)
#define LOG_VERBOSE_LED(msg)    LOG(LOG_VERBOSE, LOG_CAT_LED, msg)

/* @brief Initialize the system
 *
 * This function sets up the necessary pins and initializes serial communication. 
*/
void sysInit(LogLevel logLevel = LOG_INFO, uint8_t logCategories = LOG_CAT_ALL);

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