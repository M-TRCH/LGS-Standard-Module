#ifndef APP_H
#define APP_H

#include <Arduino.h>

/*
 * app.h - LGS application logic
 * -----------------------------
 * Keeps the high-level behaviour (operating modes + Modbus request handling)
 * out of main.cpp. The application talks to hardware only through the HAL
 * modules (led, hal_latch, hal_sensor, hal_display) and the Modbus map.
 */

/* @brief Run one iteration of the application state machine.
 *        Call this repeatedly from loop(). */
void appRun();

#endif // APP_H
