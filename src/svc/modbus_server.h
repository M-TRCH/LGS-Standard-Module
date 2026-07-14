#ifndef SVC_MODBUS_SERVER_H
#define SVC_MODBUS_SERVER_H

#include <Arduino.h>

/*  @file svc/modbus_server.h
 *  @brief Modbus RTU server service: owns the server object, the
 *         settings<->register persist table and the write-watch table.
 *
 *  App modules react to bus writes by registering handlers at init:
 *    - CHANGE rows fire once when the value differs from its shadow
 *      (shadows are seeded from the live registers, so boot never fires).
 *    - COMMAND rows fire on every poll while the coil reads 1 (level
 *      semantics, matching the original handleModbusRequests); the
 *      handler is responsible for clearing the coil.
 */

enum MbWatchKind : uint8_t
{
    MB_WATCH_REG_CHANGE,    // holding register, fire on value change
    MB_WATCH_COIL_CHANGE,   // coil, fire on value change
    MB_WATCH_COIL_COMMAND,  // coil, fire while it reads 1
};

typedef void (*MbWatchHandler)(uint16_t addr, uint16_t value);

/*  @brief Start the RTU server on the RS485 port.
 *  @param slaveId Modbus slave ID (validated range 1-247)
 *  @param baud    UART baud rate (must match rs485PortBegin) */
void modbusServerInit(uint16_t slaveId, uint32_t baud);

/*  @brief Poll the bus; on activity, dispatch the watch table. */
void modbusServerTick();

/*  @brief Register a watch row. Rows are scanned in registration order. */
void mbRegisterHandler(MbWatchKind kind, uint16_t addr, MbWatchHandler handler);

/*  @brief Seed all CHANGE shadows from the live register/coil values.
 *         Call once after mbSettingsToRegisters() so boot values never
 *         count as changes. */
void mbWatchSeedShadows();

// Thin typed accessors so app code never names the library object.
uint16_t mbRegRead(uint16_t addr);
void mbRegWrite(uint16_t addr, uint16_t value);
bool mbCoilRead(uint16_t addr);
void mbCoilWrite(uint16_t addr, bool value);

/*  @brief Publish persisted settings + compile-time identity into the
 *         Modbus registers (persist table, direction: settings -> regs). */
void mbSettingsToRegisters();

/*  @brief Collect the R/W(F) registers into the settings service
 *         (persist table, direction: regs -> settings).
 *  @param save Persist to the AT24 when true */
void mbRegistersToSettings(bool save = true);

#endif // SVC_MODBUS_SERVER_H
