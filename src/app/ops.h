#ifndef APP_OPS_H
#define APP_OPS_H

#include <Arduino.h>

/*  @file app/ops.h
 *  @brief Operation coils (500-504): factory resets, write-to-flash and
 *         software reset. Isolated because every path ends in a system
 *         reset.
 */

/*  @brief Register the Modbus handlers for the operation coils. */
void opsInit();

/*  @brief Reset the MCU after forcing outputs to a safe state.
 *
 *  Single funnel for every reset path so the latch MOSFET can never be
 *  left floating across the reset.
 */
void opsSystemReset() __attribute__((noreturn));

#endif // APP_OPS_H
