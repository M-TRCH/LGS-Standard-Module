#ifndef MODBUS_UTILS_H
#define MODBUS_UTILS_H

#include <Arduino.h>
#include <ModbusRTUServer.h>
#include "system.h"
#include "led.h"
#include "eeprom_utils.h"
#include "modbus_map.h"     // Register / coil address map (enums + helpers)

#define COIL_NUM                    5000    // Number of coils
#define DISCRETE_INPUT_NUM          1       // Number of discrete inputs
#define HOLDING_REGISTER_NUM        400     // Number of holding registers
#define INPUT_REGISTER_NUM          1       // Number of input registers

// Global variables for Modbus
extern uint16_t last_global_brightness;
extern uint16_t last_global_max_on_time;

// Modbus RTU Server Object
extern ModbusRTUServerClass RTUServer;

// Modbus Initialization
void modbusInit(int id=DEFAULT_IDENTIFIER);

// Modbus to EEPROM Mapping
void modbus2eepromMapping(bool saveEEPROM=true);

// EEPROM to Modbus Mapping
void eeprom2modbusMapping(bool loadEEPROM=false);

#endif