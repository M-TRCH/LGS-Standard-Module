#ifndef APP_DIAG_CONTROL_H
#define APP_DIAG_CONTROL_H

#include <Arduino.h>

/*  @file app/diag_control.h
 *  @brief Remote diagnostics on the Modbus surface (regs 5-11):
 *         uptime, boot counter, last reset cause, health bitfield,
 *         function mode and the active LED preset — plus the temperature
 *         sensor fault sentinel for regs 20/21.
 *
 *  Everything here is read-only on the wire and refreshed once per second;
 *  the goal is that a master can see from the bus what previously required
 *  standing in front of the board (AT24 fault blink, dead sensor, which
 *  preset is lit, unexpected watchdog reboots).
 */

/*  @brief Capture the reset cause (RCC->CSR, then clears the flags) and
 *         publish the static registers.
 *  @param oledPresent  OLED probe result (health bit 1)
 *  @param functionMode boot mode as reported at reg 10 (FunctionSwitchMode) */
void diagControlInit(bool oledPresent, uint8_t functionMode);

/*  @brief Publish uptime + health once per second. Call every loop. */
void diagControlTick(uint32_t now);

/*  @brief Report a temperature read attempt (idx 0 = room/reg 20,
 *         1 = board/reg 21). After TEMP_FAIL_THRESHOLD consecutive
 *         failures the register shows TEMP_SENTINEL and the health bit
 *         drops; one success recovers both. */
void diagReportSensor(uint8_t idx, bool ok);

/*  @brief Boot counter feed (owned by the stats blob in led_control). */
void diagSetBootCount(uint16_t count);

#endif // APP_DIAG_CONTROL_H
