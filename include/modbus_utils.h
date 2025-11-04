#ifndef MODBUS_UTILS_H
#define MODBUS_UTILS_H

#include <Arduino.h>
#include <ModbusRTUServer.h>
#include "system.h"
#include "led.h"
#include "eeprom_utils.h"

#define COIL_NUM                    5000    // Number of coils
#define DISCRETE_INPUT_NUM          1       // Number of discrete inputs
#define HOLDING_REGISTER_NUM        400     // Number of holding registers
#define INPUT_REGISTER_NUM          1       // Number of input registers

// Modbus Address Mapping (Updated on 28-08-25)
// (1) Device Information (Holding Registers, EEPROM Area)
#define MB_REG_DEVICE_TYPE          0       // device type (read-only)
#define MB_REG_FW_VERSION           1       // firmware version (read-only)
#define MB_REG_HW_VERSION           2       // hardware version (read-only)
#define MB_REG_BAUD_RATE            3       // baud rate (default 9600) (read/write)
#define MB_REG_IDENTIFIER           4       // identifier (read/write, range 1-246, default 247)

// (2) Device Control (Coils, EEPROM Area)
#define MB_COIL_FACTORY_RESET                   500     // Factory Reset (write)
#define MB_COIL_APPLY_FACTORY_RESET_EXCEPT_ID   501     // Factory Reset Except ID (write)
#define MB_COIL_APPLY_FACTORY_RESET_ALL_DATA    502     // Apply Factory Reset (write)
#define MB_COIL_WRITE_TO_EEPROM                 503     // Write to EEPROM (write)
#define MB_COIL_SOFTWARE_RESET                  504     // Software Reset (write)

// (3) LED Configuration (Holding Registers, EEPROM Area)
// Brightness: read/write, range 0-100, default 20
// Red, Green, Blue: read/write, range 0-255
// LED1: red (R=255, G=0, B=0)
#define MB_REG_LED_1_BRIGHTNESS     110
#define MB_REG_LED_1_RED            111
#define MB_REG_LED_1_GREEN          112
#define MB_REG_LED_1_BLUE           113
#define MB_REG_LED_1_MAX_ON_TIME    114
// LED2: green (R=0, G=255, B=0)
#define MB_REG_LED_2_BRIGHTNESS     120
#define MB_REG_LED_2_RED            121
#define MB_REG_LED_2_GREEN          122
#define MB_REG_LED_2_BLUE           123
#define MB_REG_LED_2_MAX_ON_TIME    124
// LED3: blue (R=0, G=0, B=255)
#define MB_REG_LED_3_BRIGHTNESS     130
#define MB_REG_LED_3_RED            131
#define MB_REG_LED_3_GREEN          132
#define MB_REG_LED_3_BLUE           133
#define MB_REG_LED_3_MAX_ON_TIME    134
// LED4: yellow (R=255, G=255, B=0)
#define MB_REG_LED_4_BRIGHTNESS     140
#define MB_REG_LED_4_RED            141
#define MB_REG_LED_4_GREEN          142
#define MB_REG_LED_4_BLUE           143
#define MB_REG_LED_4_MAX_ON_TIME    144
// LED5: cyan (R=0, G=255, B=255)
#define MB_REG_LED_5_BRIGHTNESS     150
#define MB_REG_LED_5_RED            151
#define MB_REG_LED_5_GREEN          152
#define MB_REG_LED_5_BLUE           153
#define MB_REG_LED_5_MAX_ON_TIME    154
// LED6: purple (R=255, G=0,   B=255)
#define MB_REG_LED_6_BRIGHTNESS     160
#define MB_REG_LED_6_RED            161
#define MB_REG_LED_6_GREEN          162
#define MB_REG_LED_6_BLUE           163
#define MB_REG_LED_6_MAX_ON_TIME    164
// LED7: orange (R=255, G=165, B=0)
#define MB_REG_LED_7_BRIGHTNESS     170
#define MB_REG_LED_7_RED            171
#define MB_REG_LED_7_GREEN          172
#define MB_REG_LED_7_BLUE           173
#define MB_REG_LED_7_MAX_ON_TIME    174
// LED8: white (R=255, G=255, B=255)
#define MB_REG_LED_8_BRIGHTNESS     180
#define MB_REG_LED_8_RED            181
#define MB_REG_LED_8_GREEN          182
#define MB_REG_LED_8_BLUE           183
#define MB_REG_LED_8_MAX_ON_TIME    184

#define MB_REG_GLOBAL_BRIGHTNESS    200
#define MB_REG_UNDEFINED_201        201
#define MB_REG_UNDEFINED_202        202
#define MB_REG_UNDEFINED_203        203
#define MB_REG_GLOBAL_MAX_ON_TIME   204

// (4) LED Statistics (Holding Registers, RAM Area)
#define MB_REG_LED_1_ON_COUNTER     210
#define MB_REG_LED_1_ON_TIME        211
#define MB_REG_LED_2_ON_COUNTER     220
#define MB_REG_LED_2_ON_TIME        221
#define MB_REG_LED_3_ON_COUNTER     230
#define MB_REG_LED_3_ON_TIME        231
#define MB_REG_LED_4_ON_COUNTER     240
#define MB_REG_LED_4_ON_TIME        241
#define MB_REG_LED_5_ON_COUNTER     250
#define MB_REG_LED_5_ON_TIME        251
#define MB_REG_LED_6_ON_COUNTER     260
#define MB_REG_LED_6_ON_TIME        261
#define MB_REG_LED_7_ON_COUNTER     270
#define MB_REG_LED_7_ON_TIME        271
#define MB_REG_LED_8_ON_COUNTER     280
#define MB_REG_LED_8_ON_TIME        281

// (5) LED Control (Coils, RAM Area)
#define MB_COIL_LED_1_ENABLE        1001
#define MB_COIL_LED_2_ENABLE        1002
#define MB_COIL_LED_3_ENABLE        1003
#define MB_COIL_LED_4_ENABLE        1004
#define MB_COIL_LED_5_ENABLE        1005
#define MB_COIL_LED_6_ENABLE        1006
#define MB_COIL_LED_7_ENABLE        1007
#define MB_COIL_LED_8_ENABLE        1008

extern ModbusRTUServerClass RTUServer;

// Modbus Initialization
void modbusInit(int id=DEFAULT_IDENTIFIER);

// Modbus to EEPROM Mapping
void modbus2eepromMapping(bool saveEEPROM=true);

// EEPROM to Modbus Mapping
void eeprom2modbusMapping(bool loadEEPROM=false);

#endif