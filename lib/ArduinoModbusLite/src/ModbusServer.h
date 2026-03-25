/*
  This file is part of the ArduinoModbusLite library.
  Based on ArduinoModbus - Copyright (c) 2018 Arduino SA. All rights reserved.

  SPDX-License-Identifier: LGPL-2.1+
*/

#ifndef _MODBUS_SERVER_H_INCLUDED
#define _MODBUS_SERVER_H_INCLUDED

#include <Arduino.h>

extern "C" {
  #include "libmodbus/modbus.h"
}

class ModbusServer {

public:
  int configureCoils(int startAddress, int nb);
  int configureDiscreteInputs(int startAddress, int nb);
  int configureHoldingRegisters(int startAddress, int nb);
  int configureInputRegisters(int startAddress, int nb);

  int coilRead(int address);
  int discreteInputRead(int address);
  long holdingRegisterRead(int address);
  long inputRegisterRead(int address);
  int coilWrite(int address, uint8_t value);
  int holdingRegisterWrite(int address, uint16_t value);
  int registerMaskWrite(int address, uint16_t andMask, uint16_t orMask);
  int discreteInputWrite(int address, uint8_t value);
  int writeDiscreteInputs(int address, uint8_t values[], int nb);
  int inputRegisterWrite(int address, uint16_t value);
  int writeInputRegisters(int address, uint16_t values[], int nb);

  virtual int poll() = 0;
  void end();

protected:
  ModbusServer();
  virtual ~ModbusServer();

  int begin(modbus_t* _mb, int id);

protected:
  modbus_t* _mb;
  modbus_mapping_t _mbMapping;
};

#endif
