/*
  This file is part of the ArduinoModbusLite library.
  Based on ArduinoModbus - Copyright (c) 2018 Arduino SA. All rights reserved.

  SPDX-License-Identifier: LGPL-2.1+
*/

#ifndef _MODBUS_RTU_SERVER_H_INCLUDED
#define _MODBUS_RTU_SERVER_H_INCLUDED

#include "ModbusServer.h"
#include <ArduinoRS485.h>

class ModbusRTUServerClass : public ModbusServer {
public:
  ModbusRTUServerClass();
  ModbusRTUServerClass(RS485Class& rs485);
  virtual ~ModbusRTUServerClass();

  int begin(int id, unsigned long baudrate, uint16_t config = SERIAL_8N1);
  int begin(RS485Class& rs485, int id, unsigned long baudrate, uint16_t config = SERIAL_8N1);

  virtual int poll();

private:
  RS485Class* _rs485 = &RS485;
};

extern ModbusRTUServerClass ModbusRTUServer;

#endif
