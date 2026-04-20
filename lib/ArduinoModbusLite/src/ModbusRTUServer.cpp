/*
  This file is part of the ArduinoModbusLite library.
  Based on ArduinoModbus - Copyright (c) 2018 Arduino SA. All rights reserved.

  SPDX-License-Identifier: LGPL-2.1+
*/

#include <errno.h>

extern "C" {
#include "libmodbus/modbus.h"
#include "libmodbus/modbus-rtu.h"
}

#include "ModbusRTUServer.h"

ModbusRTUServerClass::ModbusRTUServerClass()
{
}

ModbusRTUServerClass::ModbusRTUServerClass(RS485Class& rs485) : _rs485(&rs485)
{
}

ModbusRTUServerClass::~ModbusRTUServerClass()
{
}

int ModbusRTUServerClass::begin(int id, unsigned long baudrate, uint16_t config)
{
  modbus_t* mb = modbus_new_rtu(_rs485, baudrate, config);

  if (!ModbusServer::begin(mb, id)) {
    return 0;
  }

  modbus_connect(mb);
  return 1;
}

int ModbusRTUServerClass::begin(RS485Class& rs485, int id, unsigned long baudrate, uint16_t config)
{
  _rs485 = &rs485;
  return begin(id, baudrate, config);
}

void ModbusRTUServerClass::setResponseDelay(unsigned long delayMs)
{
  _responseDelayMs = delayMs;
}

unsigned long ModbusRTUServerClass::getResponseDelay() const
{
  return _responseDelayMs;
}

int ModbusRTUServerClass::poll()
{
  uint8_t request[MODBUS_RTU_MAX_ADU_LENGTH];

  int requestLength = modbus_receive(_mb, request);

  if (requestLength > 0) {
    // Insert response delay to allow slow RS485 hub auto-direction circuits to switch
    if (_responseDelayMs > 0) {
      delay(_responseDelayMs);
    }
    modbus_reply(_mb, request, requestLength, &_mbMapping);
    return 1;
  }
  return 0;
}

ModbusRTUServerClass ModbusRTUServer;
