
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
#define DUMMY_PIN       PA0
#define LED_RUN_PIN     PB9
#define LED1_PIN        PB1
#define LED2_PIN        PB2
#define LED3_PIN        PA11
#define LED4_PIN        PA8
#define LED5_PIN        PB0
#define LED6_PIN        PC13
#define LED7_PIN        PB14
#define LED8_PIN        PB15
#define ROW_SW_PIN      PD1
#define COL_SW_PIN      PD0
#define MOSFET_PIN      PB4
#define SERVO_PIN       PA7
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

void sysInit();

#endif