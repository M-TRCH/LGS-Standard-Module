#ifndef APP_OTA_CONTROL_H
#define APP_OTA_CONTROL_H

#include <Arduino.h>

/*  @file app/ota_control.h
 *  @brief OTA-over-RS485 session: Modbus surface (regs 282-389, coils
 *         505-508), staging download, verification and the apply handoff
 *         to the bootloader.
 *
 *  Transport is Modbus broadcast FC16 (slave id 0): the master streams
 *  128-byte chunks into the data window (regs 290-357) and bumps the commit
 *  register on every transmission; devices track received chunks in a
 *  bitmap (regs 360-389) the master reads back per device to re-send only
 *  what was lost. Flow:
 *
 *    metadata (regs 284-288) -> coil 505 enter (staging erase) -> chunks ->
 *    bitmap repair rounds -> coil 506 finalize (CRC32 verify) ->
 *    coil 507 apply (header commit + reset; bootloader copies)
 *
 *  Session states (reg 282 lo byte): 0 idle, 1 receiving, 2 verified,
 *  3 failed (hi byte = error code). A session with no bus activity for
 *  OTA_SESSION_TIMEOUT_MS fails out and returns to idle.
 */

/*  @brief Register the OTA Modbus handlers. */
void otaControlInit();

/*  @brief Session upkeep (inactivity timeout). Call once per loop. */
void otaControlTick(uint32_t now);

#endif // APP_OTA_CONTROL_H
