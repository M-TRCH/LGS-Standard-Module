#ifndef MODBUS_UTILS_H
#define MODBUS_UTILS_H

#include <Arduino.h>
#include <ModbusRTUServer.h>
#include "config.h"
#include "system.h"

#define MODBUS_ID                   18      // Slave Identifier
#define COIL_NUM                    800     // Number of coils
#define DISCRETE_INPUT_NUM          800     // Number of discrete inputs
#define HOLDING_REGISTER_NUM        800     // Number of holding registers
#define INPUT_REGISTER_NUM          800     // Number of input registers

// Modbus Address Mapping
// Group A: Device and Statistic (Holding Registers, EEPROM Area)
#define MB_REG_DEVICE_TYPE          0       // device type (read-only)
#define MB_REG_FW_VERSION           1       // firmware version (read-only)
#define MB_REG_SERIAL_NUMBER        2       // serial number (read-only)
#define MB_REG_BAUD_RATE            3       // baud rate (default 9600) (read/write)
#define MB_REG_IDENTIFIER           4       // identifier (read/write, range 1-246, default 247)

// Group B: System Control (Coils, EEPROM Area)
#define MB_COIL_FACTORY_RESET       0       // Factory Reset (write)
#define MB_COIL_WRITE_TO_EEPROM     1       // Write to EEPROM (write)

// Group C: LED Configuration (Holding Registers, EEPROM Area)
// Brightness: read/write, range 0-100, default 20
// Red, Green, Blue: read/write, range 0-255
// LED0: red (R=255, G=0, B=0)
#define MB_REG_LED_0_BRIGHTNESS     100
#define MB_REG_LED_0_RED            101
#define MB_REG_LED_0_GREEN          102
#define MB_REG_LED_0_BLUE           103

// LED1: green (R=0, G=255, B=0)
#define MB_REG_LED_1_BRIGHTNESS     110
#define MB_REG_LED_1_RED            111
#define MB_REG_LED_1_GREEN          112
#define MB_REG_LED_1_BLUE           113

// LED2: blue (R=0, G=0, B=255)
#define MB_REG_LED_2_BRIGHTNESS     120
#define MB_REG_LED_2_RED            121
#define MB_REG_LED_2_GREEN          122
#define MB_REG_LED_2_BLUE           123

// LED3: yellow (R=255, G=255, B=0)
#define MB_REG_LED_3_BRIGHTNESS     130
#define MB_REG_LED_3_RED            131
#define MB_REG_LED_3_GREEN          132
#define MB_REG_LED_3_BLUE           133

// LED4: cyan (R=0, G=255, B=255)
#define MB_REG_LED_4_BRIGHTNESS     140
#define MB_REG_LED_4_RED            141
#define MB_REG_LED_4_GREEN          142
#define MB_REG_LED_4_BLUE           143

// LED5: purple (R=255, G=0,   B=255)
#define MB_REG_LED_5_BRIGHTNESS     150
#define MB_REG_LED_5_RED            151
#define MB_REG_LED_5_GREEN          152
#define MB_REG_LED_5_BLUE           153

// LED6: orange (R=255, G=165, B=0)
#define MB_REG_LED_6_BRIGHTNESS     160
#define MB_REG_LED_6_RED            161
#define MB_REG_LED_6_GREEN          162
#define MB_REG_LED_6_BLUE           163

// LED7: white (R=255, G=255, B=255)
#define MB_REG_LED_7_BRIGHTNESS     170
#define MB_REG_LED_7_RED            171
#define MB_REG_LED_7_GREEN          172
#define MB_REG_LED_7_BLUE           173

// Group R: LED Control (Coils, RAM Area)
#define MB_COIL_LED_0_ENABLE        100
#define MB_COIL_LED_1_ENABLE        110
#define MB_COIL_LED_2_ENABLE        120
#define MB_COIL_LED_3_ENABLE        130
#define MB_COIL_LED_4_ENABLE        140
#define MB_COIL_LED_5_ENABLE        150
#define MB_COIL_LED_6_ENABLE        160
#define MB_COIL_LED_7_ENABLE        170

// Group S: System status (Reg, RAM Area)
#define MB_REG_LAST_ERROR           10      // last error (read-only)

extern ModbusRTUServerClass RTUServer;

void modbusInit();

#endif