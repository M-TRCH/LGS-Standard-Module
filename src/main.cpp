// LGS Standard Module - Firmware Entry Point
// Reference Documentation: LGS-Control-Table-Updated-271125.pdf
//
// The application logic lives in the app module (app.h / app.cpp), which
// orchestrates the system, EEPROM, LED and Modbus modules. This file only
// wires the Arduino framework hooks to the application layer.

#include <Arduino.h>
#include "app.h"

void setup()
{
    appInit();  // Initialize all subsystems
}

void loop()
{
    appRun();   // Run one iteration of the application loop
}
