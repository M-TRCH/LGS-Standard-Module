
#ifndef SYSTEM_H
#define SYSTEM_H

#include <Arduino.h>
#include <Stream.h>
#include <HardwareSerial.h>
#include <ArduinoRS485.h>
#include <Wire.h>
#include <SensirionI2CSts4x.h>

// Pin definitions
#define RX_PIN          PA10 
#define TX_PIN          PA9
#define RX3_PIN         PA3
#define TX3_PIN         PA2
#define DUMMY_PIN       PA1     // (New) dummy pin for rs485
#define SCL1_PIN        PB8     // (New) I2C1 SCL pin
#define SDA1_PIN        PB9     // (New) I2C1 SDA pin
#define LED_RUN_PIN     PA15    // (New) Run LED pin
#define LED1_PIN        PB1 
#define LED2_PIN        PB2     
#define LED3_PIN        PA11     
#define LED4_PIN        PA8
#define LED5_PIN        PB0
#define LED6_PIN        PC13
#define LED7_PIN        PB14
#define LED8_PIN        PB15
#define FUNC_SW_PIN     PA0     // (New) Function switch pin
#define MOSFET_PIN      PB4     
#define SENSE_PIN       PA6

// Communication settings
#define DEBUG_BAUD      9600
#define MODBUS_BAUD     9600

// System settings
#define LED_BLINK_MS    500     // LED blink interval in milliseconds

// Global variables
extern bool run_led_state;

// Object declarations
extern HardwareSerial Serial3; 
extern RS485Class rs485;
extern RS485Class rs4853;
extern SensirionI2CSts4x sts4x;

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

/* @brief Check if the latch is locked
 *
 * This function reads the state of the latch and returns true if it is locked, false otherwise.
 *
 * @return true if the latch is locked, false otherwise
 */
bool isLatchLocked(int debounceDelay = 20);

#endif