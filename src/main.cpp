/*
 * main.cpp - LGS (Light Guiding Shelf) firmware entry point
 * ---------------------------------------------------------
 * Thin entry point only. All hardware access lives in the HAL modules
 * (led, hal_latch, hal_sensor, hal_display) and all behaviour lives in app.cpp.
 *
 * Reference: doc/MODBUS_CONTROL_TABLE.md (generated from LGS-Control-Table.xlsx)
 */

#include <Arduino.h>
#include "system.h"
#include "led.h"
#include "eeprom_utils.h"
#include "modbus_utils.h"
#include "hal/hal_display.h"
#include "app.h"

void setup()
{
    sysInit(LOG_NONE);      // System core: serial, logging, HAL init, function switch
    eepromInit();           // Load persistent configuration
    ledInit();              // Channel LED strips
    displayInit();          // Optional OLED (no-op unless LGS_ENABLE_DISPLAY=1)

    // Start Modbus server. SET_ID mode uses a temporary special ID (246).
    modbusInit(functionMode == FUNC_SW_SET_ID ? DEFAULT_IDENTIFIER - 1 : eepromConfig.identifier);
    eeprom2modbusMapping(); // Publish EEPROM config to Modbus registers
}

void loop()
{
    appRun();               // One iteration of the application state machine
}
