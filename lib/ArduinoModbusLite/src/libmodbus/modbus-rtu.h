/*
 * Copyright © 2001-2011 Stéphane Raimbault <stephane.raimbault@gmail.com>
 * Copyright © 2018 Arduino SA. All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include "modbus.h"

MODBUS_BEGIN_DECLS

#define MODBUS_RTU_MAX_ADU_LENGTH  256

#ifdef ARDUINO
class RS485Class;
MODBUS_API modbus_t* modbus_new_rtu(RS485Class *rs485, unsigned long baud, uint16_t config);
#endif

MODBUS_END_DECLS

#endif /* MODBUS_RTU_H */
