/*
 * Copyright © 2001-2011 Stéphane Raimbault <stephane.raimbault@gmail.com>
 * Copyright © 2018 Arduino SA. All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1+
 *
 * Stripped for ArduinoModbusLite: Arduino-only.
 */

#ifndef MODBUS_RTU_PRIVATE_H
#define MODBUS_RTU_PRIVATE_H

#include <stdint.h>
#include <ArduinoRS485.h>

#define _MODBUS_RTU_HEADER_LENGTH      1
#define _MODBUS_RTU_PRESET_REQ_LENGTH  6
#define _MODBUS_RTU_PRESET_RSP_LENGTH  2

#define _MODBUS_RTU_CHECKSUM_LENGTH    2

typedef struct _modbus_rtu {
    unsigned long baud;
    uint16_t config;
    RS485Class* rs485;
    int confirmation_to_ignore;
} modbus_rtu_t;

#endif /* MODBUS_RTU_PRIVATE_H */
