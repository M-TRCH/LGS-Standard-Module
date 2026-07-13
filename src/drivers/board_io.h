#ifndef DRIVERS_BOARD_IO_H
#define DRIVERS_BOARD_IO_H

#include <Arduino.h>
#include "board.h"

/*  @file drivers/board_io.h
 *  @brief Discrete board I/O: RUN LED, function switch, latch MOSFET/sense.
 *
 *  Pin-level access only — policy (debounce, pulse limits, mode selection)
 *  lives in the app layer.
 */

/*  @brief Configure the discrete I/O pins and drive safe defaults
 *         (RUN LED off, latch MOSFET off). */
void boardIoInit();

/*  @brief Drive the RUN status LED. */
void boardSetRunLed(bool on);

/*  @brief Read the function switch (active LOW).
 *  @return true while the switch is pressed */
bool boardFunctionSwitchPressed();

/*  @brief Drive the latch MOSFET gate. true = energize (unlock). */
void boardLatchMosfetSet(bool on);

/*  @brief Raw latch sense input.
 *  @return true while the sense pin reads LOW (latch present/locked) */
bool boardLatchSenseLow();

#endif // DRIVERS_BOARD_IO_H
