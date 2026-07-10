#ifndef APP_H
#define APP_H

/*  @file app.h
 *  @brief Application layer for the LGS Standard Module.
 *
 *  This module is the top-level orchestrator. It wires together the lower-level
 *  modules (system, EEPROM, LED, Modbus) and implements the runtime state
 *  machine driven by the function-switch mode. Keeping this logic isolated from
 *  main.cpp makes the firmware easier to extend with new operating modes or
 *  features in the future.
 *
 *  main.cpp only needs to call appInit() once and appRun() every loop.
 */

/*  @brief Initialize all subsystems.
 *
 *  Brings up the system core, EEPROM configuration, LED strips and the Modbus
 *  RTU server, then maps the stored configuration into the Modbus registers.
 */
void appInit();

/*  @brief Execute one iteration of the main application loop.
 *
 *  Dispatches to the handler for the currently selected function-switch mode
 *  and services pending Modbus requests.
 */
void appRun();

#endif // APP_H
