#ifndef HW_CONFIG_H
#define HW_CONFIG_H

/*
 * hw_config.h - Hardware abstraction of board pin mapping
 * -------------------------------------------------------
 * All physical pin assignments live here so that the application logic
 * (main.cpp, app.cpp, hal/*) never references a raw MCU pin directly.
 *
 * Select the target board with a PlatformIO build flag, e.g.:
 *     build_flags = -D LGS_BOARD_STM32F103    ; current production board (R4.x)
 *     build_flags = -D LGS_BOARD_STM32G030    ; next-gen board
 *
 * If no board flag is provided, STM32F103 is assumed for backward
 * compatibility with existing build scripts.
 */

#if !defined(LGS_BOARD_STM32F103) && !defined(LGS_BOARD_STM32G030)
    #define LGS_BOARD_STM32F103
#endif

// ---------------------------------------------------------------------------
// Board: STM32F103C8T6  (LGS hardware R4.0.1 - R4.3)
// ---------------------------------------------------------------------------
#if defined(LGS_BOARD_STM32F103)

    #define LGS_BOARD_NAME      "STM32F103C8T6"

    // Debug / RS485 primary UART (USART1)
    #define RX_PIN          PA10
    #define TX_PIN          PA9

    // Modbus RS485 secondary UART (USART2)
    #define RX3_PIN         PA3
    #define TX3_PIN         PA2
    #define DUMMY_PIN       PA1     // Unused DE/RE placeholder for RS485 driver

    // I2C1 (built-in temperature sensor / OLED)
    #define SCL1_PIN        PB8
    #define SDA1_PIN        PB9

    // Status / channel LEDs
    #define LED_RUN_PIN     PA15    // RUN status LED
    #define LED1_PIN        PB1
    #define LED2_PIN        PB2
    #define LED3_PIN        PA11
    #define LED4_PIN        PA8
    #define LED5_PIN        PB0
    #define LED6_PIN        PC13
    #define LED7_PIN        PB14
    #define LED8_PIN        PB15

    // User I/O
    #define FUNC_SW_PIN     PA0     // Function switch (active LOW)
    #define MOSFET_PIN      PB4     // Electronic latch drive (NPN MOSFET)
    #define SENSE_PIN       PA6     // Latch state sense (active LOW = locked)

// ---------------------------------------------------------------------------
// Board: STM32G030C8T6  (next-gen LGS hardware - provisional mapping)
// ---------------------------------------------------------------------------
// NOTE: The STM32G0 family exposes USART1/USART2 (no USART3). The pin numbers
//       below mirror the F103 layout where an equivalent pin exists and MUST be
//       verified against the final STM32G030 schematic before production.
#elif defined(LGS_BOARD_STM32G030)

    #define LGS_BOARD_NAME      "STM32G030C8T6"

    // Debug / RS485 primary UART (USART1)
    #define RX_PIN          PA10
    #define TX_PIN          PA9

    // Modbus RS485 secondary UART (USART2)
    #define RX3_PIN         PA3
    #define TX3_PIN         PA2
    #define DUMMY_PIN       PA1

    // I2C1 (built-in temperature sensor / OLED)
    #define SCL1_PIN        PB8
    #define SDA1_PIN        PB9

    // Status / channel LEDs
    #define LED_RUN_PIN     PA15
    #define LED1_PIN        PB1
    #define LED2_PIN        PB2
    #define LED3_PIN        PA11
    #define LED4_PIN        PA8
    #define LED5_PIN        PB0
    #define LED6_PIN        PC13
    #define LED7_PIN        PB14
    #define LED8_PIN        PB15

    // User I/O
    #define FUNC_SW_PIN     PA0
    #define MOSFET_PIN      PB4
    #define SENSE_PIN       PA6

#endif

#endif // HW_CONFIG_H
