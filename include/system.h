
#ifndef SYSTEM_H
#define SYSTEM_H

#include <Arduino.h>
#include <Stream.h>
#include <HardwareSerial.h>
#include <ArduinoRS485.h>

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

// Constants definitions
// Debug level
enum DebugLevel
{
    DEBUG_NONE = 0,
    DEBUG_BASIC,
    DEBUG_VERBOSE
};

extern DebugLevel debugLevel;

// Macro definitions
#define PRINT(level, msg) \
    do { if (debugLevel >= level) Serial.print(msg); } while(0)


/* @brief Initialize the system
 *
 * This function sets up the necessary pins and initializes serial communication. 
*/
void sysInit();

/* @brief Check if the latch is locked
 *
 * This function reads the state of the latch and returns true if it is locked, false otherwise.
 *
 * @return true if the latch is locked, false otherwise
 */
bool isLatchLocked(int debounceDelay = 20);

#endif