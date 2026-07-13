#ifndef HW_CONFIG_H
#define HW_CONFIG_H

#include <Arduino.h>

// Current board hardware map

// UART Debug
#define HW_UART_DEBUG_TX_PIN           PA2

// UART RS485
#define HW_UART_RS485_RX_PIN           PB7
#define HW_UART_RS485_TX_PIN           PA9
#define HW_UART_RS485_DE_PIN           PD0

// I2C - internal devices
#define HW_I2C1_SDA_PIN                PB9
#define HW_I2C1_SCL_PIN                PB6

// I2C - OLED 0.96"
#define HW_I2C2_SDA_PIN                PA12
#define HW_I2C2_SCL_PIN                PB13

// LEDs
#define HW_LED_BUILTIN_PIN             PA10
#define HW_LED_RING_PIN                PA8
#define HW_LED_RING_PIXEL_COUNT        16

// Inputs / outputs
#define HW_FUNCTION_SWITCH_PIN         PA7
#define HW_LATCH_TRIGGER_PIN           PB3
#define HW_LATCH_CHECK_PIN             PA3

// Reserved for future board functions
#define HW_SERVO1_PIN                  PC6
#define HW_SERVO2_PIN                  PC7

#endif