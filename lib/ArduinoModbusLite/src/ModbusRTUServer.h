/*
  This file is part of the ArduinoModbusLite library.
  Based on ArduinoModbus - Copyright (c) 2018 Arduino SA. All rights reserved.

  SPDX-License-Identifier: LGPL-2.1+
*/

#ifndef _MODBUS_RTU_SERVER_H_INCLUDED
#define _MODBUS_RTU_SERVER_H_INCLUDED

#include "ModbusServer.h"
#include <ArduinoRS485.h>

// Default response delay in milliseconds (workaround for slow RS485 hub auto-direction switching)
#define MODBUS_RTU_DEFAULT_RESPONSE_DELAY_MS 5

class ModbusRTUServerClass : public ModbusServer {
public:
  ModbusRTUServerClass();
  ModbusRTUServerClass(RS485Class& rs485);
  virtual ~ModbusRTUServerClass();

  int begin(int id, unsigned long baudrate, uint16_t config = SERIAL_8N1);
  int begin(RS485Class& rs485, int id, unsigned long baudrate, uint16_t config = SERIAL_8N1);

  /**
   * Set the response delay time (ms) inserted between receiving a request and sending the reply.
   * This gives slow RS485 hubs (auto-direction) time to switch from TX to RX.
   * @param delayMs  Delay in milliseconds (0 = no delay, default 5)
   */
  void setResponseDelay(unsigned long delayMs);
  unsigned long getResponseDelay() const;

  virtual int poll();

private:
  RS485Class* _rs485 = &RS485;
  unsigned long _responseDelayMs = MODBUS_RTU_DEFAULT_RESPONSE_DELAY_MS;
};

extern ModbusRTUServerClass ModbusRTUServer;

#endif
