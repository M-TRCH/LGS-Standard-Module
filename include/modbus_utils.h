
#ifndef MODBUS_UTILS_H
#define MODBUS_UTILS_H

#include <Arduino.h>
#include <ModbusRTUServer.h>
#include "config.h"
#include "system.h"

#define MODBUS_ID               18          // Slave Identifier
#define COIL_NUM                8           // Number of coils
#define DISCRETE_INPUT_NUM      8           // Number of discrete inputs
#define HOLDING_REGISTER_NUM    8           // Number of holding registers
#define INPUT_REGISTER_NUM      8           // Number of input registers

extern ModbusRTUServerClass RTUServer;

void modbusInit();

#endif