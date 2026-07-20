#ifndef HW_CONFIG_H
#define HW_CONFIG_H

#include <Arduino.h>

// Current board hardware map

// UART Debug
#define HW_UART_DEBUG_TX_PIN           PA2

// UART RS485 (USART1). Transceiver is auto-direction (COSMAX13487): the driver
// self-enables on TX activity and RE is tied to 3V3 in hardware, so there is no
// MCU DE/RE pin to define.
#define HW_UART_RS485_RX_PIN           PB7
#define HW_UART_RS485_TX_PIN           PA9

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

// Expansion header (3 signal pins routed to spare pads). Chosen so the trio
// covers every likely future role at once:
//   - all three together = the complete SPI1 port (SCK/MISO/MOSI)
//   - PB5 = default data pin for the SK6812MINI RGBW strip extension
//     (TIM3_CH2 PWM+DMA or SPI1_MOSI DMA upgrade paths; bit-bang today)
//   - PB4 = TIM3_CH1 (a second PWM/DMA-capable line)
//   - PA5 = USART3_TX (aux serial) / ADC1_IN5 (analog input)
// TIM3 is unused by the firmware, so both PWM channels are free; the ring
// keeps TIM1_CH1 (PA8) — two independent DMA-driven LED streams possible.
#define HW_EXP1_PIN                    PB4
#define HW_EXP2_PIN                    PB5     // SK6812 RGBW extension data (default)
#define HW_EXP3_PIN                    PA5

#endif