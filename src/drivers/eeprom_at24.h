#ifndef DRIVERS_EEPROM_AT24_H
#define DRIVERS_EEPROM_AT24_H

#include <Arduino.h>

/*  @file drivers/eeprom_at24.h
 *  @brief AT24C32D external EEPROM driver (I2C1, address 0x50, 4KB).
 *
 *  Handles the 2-byte memory addressing, 32-byte page-write boundaries and
 *  the post-write ack-polling (~5ms write cycle per page). The I2C1 bus
 *  must be initialized (boardI2C1Init) before any call.
 */

/*  @brief Check that the EEPROM answers on the bus.
 *  @return true when the device acks its address */
bool at24Probe();

/*  @brief Sequential read.
 *  @return true on success (fits the 4KB array and every chunk acked) */
bool at24Read(uint16_t addr, uint8_t *buf, size_t len);

/*  @brief Write across page boundaries, ack-polling after each page.
 *  @return true on success */
bool at24Write(uint16_t addr, const uint8_t *buf, size_t len);

#endif // DRIVERS_EEPROM_AT24_H
