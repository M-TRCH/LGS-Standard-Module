#ifndef HW_SERIAL_H
#define HW_SERIAL_H

#include <Arduino.h>
#include <ArduinoRS485.h>
#include "system.h"

/*  @file hw/serial.h
 *  @brief UART hardware driver.
 *
 *  Owns the RS485 transceiver used by the Modbus RTU server. Pin assignments
 *  come from HW_config.h.
 */

// RS485 transceiver bound to the Modbus UART (USART1)
extern RS485Class rs485;

/*  @brief Initialize the RS485 / Modbus UART. */
void serialInit();

#endif // HW_SERIAL_H
