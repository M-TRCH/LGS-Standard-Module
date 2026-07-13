#ifndef DRIVERS_RS485_PORT_H
#define DRIVERS_RS485_PORT_H

#include <Arduino.h>
#include <ArduinoRS485.h>
#include "board.h"

/*  @file drivers/rs485_port.h
 *  @brief RS485 transceiver driver for the Modbus RTU link (USART1).
 *
 *  Owns the UART and RS485 objects. The baud rate is a parameter so the
 *  caller (app layer) can supply the configured value.
 */

// RS485 transceiver bound to the Modbus UART (USART1)
extern RS485Class rs485;

/*  @brief Initialize the RS485 / Modbus UART at the given baud rate. */
void rs485PortBegin(uint32_t baud);

#endif // DRIVERS_RS485_PORT_H
