#ifndef MODBUS_UTILS_H
#define MODBUS_UTILS_H

#include <Arduino.h>
#include <ModbusRTUServer.h>
#include <ModbusRTUClient.h>
#include "system.h"
#include "led.h"
#include "eeprom_utils.h"

#define COIL_NUM                    5000    // Number of coils
#define DISCRETE_INPUT_NUM          1       // Number of discrete inputs
#define HOLDING_REGISTER_NUM        400     // Number of holding registers
#define INPUT_REGISTER_NUM          1       // Number of input registers

// Modbus Address Mapping (Updated on 12-11-25) - Draft Version
// Device groups:
#define MB_REG_DEVICE_TYPE          0       // device type (read-only)
#define MB_REG_FW_VERSION           1       // firmware version (read-only)
#define MB_REG_HW_VERSION           2       // hardware version (read-only)
#define MB_REG_BAUD_RATE            3       // baud rate (read/write/flash)
#define MB_REG_IDENTIFIER           4       // identifier (read/write/flash, range 1-246, default 247)

// Operation groups:
#define MB_COIL_FACTORY_RESET                   500     // Factory Reset (write)
#define MB_COIL_APPLY_FACTORY_RESET_EXCEPT_ID   501     // Factory Reset Except ID (write)
#define MB_COIL_APPLY_FACTORY_RESET_ALL_DATA    502     // Apply Factory Reset (write)
#define MB_COIL_WRITE_TO_EEPROM                 503     // Write to EEPROM (write)
#define MB_COIL_SOFTWARE_RESET                  504     // Software Reset (write)

// Sensor groups:
#define MB_REG_BUILT_IN_TEMP                    20      // Built-in Temperature Sensor Value (read-only)
#define MB_REG_TIME_AFTER_UNLOCK                40      // Time After Unlock (read-only)

// Configuration groups:
#define MB_REG_SET_NUM_DISPLAY      60      // Set Number of Displays (read/write)
#define MB_REG_UNLOCK_DELAY         80      // Unlock Delay Time in seconds (read/write/flash)
#define MB_REG_LED_1_BRIGHTNESS     110     // LED 1 Configuration Start Address (read/write/flash)
#define MB_REG_LED_1_RED            111
#define MB_REG_LED_1_GREEN          112
#define MB_REG_LED_1_BLUE           113
#define MB_REG_LED_1_MAX_ON_TIME    114
#define MB_REG_LED_2_BRIGHTNESS     120     // LED 2 Configuration Start Address (read/write/flash)
#define MB_REG_LED_2_RED            121
#define MB_REG_LED_2_GREEN          122
#define MB_REG_LED_2_BLUE           123
#define MB_REG_LED_2_MAX_ON_TIME    124
#define MB_REG_LED_3_BRIGHTNESS     130     // LED 3 Configuration Start Address (read/write/flash)
#define MB_REG_LED_3_RED            131
#define MB_REG_LED_3_GREEN          132
#define MB_REG_LED_3_BLUE           133
#define MB_REG_LED_3_MAX_ON_TIME    134
#define MB_REG_LED_4_BRIGHTNESS     140     // LED 4 Configuration Start Address (read/write/flash)
#define MB_REG_LED_4_RED            141
#define MB_REG_LED_4_GREEN          142
#define MB_REG_LED_4_BLUE           143
#define MB_REG_LED_4_MAX_ON_TIME    144
#define MB_REG_LED_5_BRIGHTNESS     150     // LED 5 Configuration Start Address (read/write/flash)
#define MB_REG_LED_5_RED            151
#define MB_REG_LED_5_GREEN          152
#define MB_REG_LED_5_BLUE           153
#define MB_REG_LED_5_MAX_ON_TIME    154
#define MB_REG_LED_6_BRIGHTNESS     160     // LED 6 Configuration Start Address (read/write/flash)
#define MB_REG_LED_6_RED            161
#define MB_REG_LED_6_GREEN          162
#define MB_REG_LED_6_BLUE           163
#define MB_REG_LED_6_MAX_ON_TIME    164
#define MB_REG_LED_7_BRIGHTNESS     170     // LED 7 Configuration Start Address (read/write/flash)
#define MB_REG_LED_7_RED            171
#define MB_REG_LED_7_GREEN          172
#define MB_REG_LED_7_BLUE           173
#define MB_REG_LED_7_MAX_ON_TIME    174
#define MB_REG_LED_8_BRIGHTNESS     180     // LED 8 Configuration Start Address (read/write/flash)
#define MB_REG_LED_8_RED            181
#define MB_REG_LED_8_GREEN          182
#define MB_REG_LED_8_BLUE           183
#define MB_REG_LED_8_MAX_ON_TIME    184
#define MB_REG_GLOBAL_BRIGHTNESS    190     // Global Brightness (read/write/flash)
#define MB_REG_GLOBAL_MAX_ON_TIME   194     // Global Max On Time (read/write)

// Status groups:
#define MB_REG_TOTAL_LED_ON_CNT     200     // Total LED On Counter (read-only)
#define MB_REG_TOTAL_LED_ON_TIME    201     // Total LED On Time (read-only)
#define MB_REG_LED_1_ON_COUNTER     210     // LED 1 Status Start Address (read-only)
#define MB_REG_LED_1_ON_TIME        211
#define MB_REG_LED_2_ON_COUNTER     220     // LED 2 Status Start Address (read-only)
#define MB_REG_LED_2_ON_TIME        221
#define MB_REG_LED_3_ON_COUNTER     230     // LED 3 Status Start Address (read-only)
#define MB_REG_LED_3_ON_TIME        231
#define MB_REG_LED_4_ON_COUNTER     240     // LED 4 Status Start Address (read-only)    
#define MB_REG_LED_4_ON_TIME        241
#define MB_REG_LED_5_ON_COUNTER     250     // LED 5 Status Start Address (read-only)
#define MB_REG_LED_5_ON_TIME        251
#define MB_REG_LED_6_ON_COUNTER     260     // LED 6 Status Start Address (read-only)
#define MB_REG_LED_6_ON_TIME        261
#define MB_REG_LED_7_ON_COUNTER     270     // LED 7 Status Start Address (read-only)
#define MB_REG_LED_7_ON_TIME        271
#define MB_REG_LED_8_ON_COUNTER     280     // LED 8 Status Start Address (read-only)
#define MB_REG_LED_8_ON_TIME        281
#define MB_REG_DISPLAY_ON_CNT       290     // Display On Counter (read-only)
#define MB_REG_DISPLAY_ON_TIME      291     // Display On Time (read-only)

// Control groups:
#define MB_COIL_LED_1_ENABLE        1001    // LED Enable Control Start Address (read/write)
#define MB_COIL_LED_2_ENABLE        1002
#define MB_COIL_LED_3_ENABLE        1003
#define MB_COIL_LED_4_ENABLE        1004
#define MB_COIL_LED_5_ENABLE        1005
#define MB_COIL_LED_6_ENABLE        1006
#define MB_COIL_LED_7_ENABLE        1007
#define MB_COIL_LED_8_ENABLE        1008

// Modbus Client Configuration
#define MODBUS_BROADCAST_ID         0       // Broadcast address
#define MODBUS_COIL_START_ADDR      1001    // Start address for coils (LED 1-8 Enable)
#define MODBUS_COIL_COUNT           8       // Number of coils to write
#define MB_COIL_DISPLAY_ENABLE      1010    // Display Enable Control Start Address (read/write)
#define MB_COIL_LED_1_DISPLAY       1011
#define MB_COIL_LED_2_DISPLAY       1012
#define MB_COIL_LED_3_DISPLAY       1013
#define MB_COIL_LED_4_DISPLAY       1014
#define MB_COIL_LED_5_DISPLAY       1015
#define MB_COIL_LED_6_DISPLAY       1016
#define MB_COIL_LED_7_DISPLAY       1017
#define MB_COIL_LED_8_DISPLAY       1018
#define MB_COIL_LATCH_TRIGGER       1020    // Latch Trigger Control Start Address (read/write)
#define MB_COIL_LED_1_LATCH         1021
#define MB_COIL_LED_2_LATCH         1022
#define MB_COIL_LED_3_LATCH         1023    
#define MB_COIL_LED_4_LATCH         1024
#define MB_COIL_LED_5_LATCH         1025
#define MB_COIL_LED_6_LATCH         1026
#define MB_COIL_LED_7_LATCH         1027
#define MB_COIL_LED_8_LATCH         1028

// Global variables for Modbus
extern uint16_t last_global_brightness;
extern uint16_t last_global_max_on_time;

// ===== Modbus RTU Server Functions (Commented Out) =====
// Modbus RTU Server Object
// extern ModbusRTUServerClass RTUServer;

// Modbus Server Initialization
// void modbusInit(int id=DEFAULT_IDENTIFIER);

// Modbus to EEPROM Mapping
// void modbus2eepromMapping(bool saveEEPROM=true);

// EEPROM to Modbus Mapping
// void eeprom2modbusMapping(bool loadEEPROM=false);

// ===== Modbus RTU Client Functions =====
// Modbus RTU Client Object
extern ModbusRTUClientClass RTUClient;

// Modbus Client Initialization
void modbusClientInit();

// Broadcast LED control operation
// ledStates: array of 8 boolean values for LED 1-8 enable status
void broadcastOperate(bool ledStates[8]);

// Broadcast single LED control
void broadcastSingleLed(uint8_t ledNumber, bool state);

// Broadcast all LEDs with same state
void broadcastAllLeds(bool state);

#endif