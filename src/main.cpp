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

// ---------------------------------------------------------------------------
// OLED test scaffolding (kept for bring-up / debugging).
// Uncomment and add "#include <Adafruit_GFX.h>" / "#include <Adafruit_SSD1306.h>"
// to exercise the display during development.
// ---------------------------------------------------------------------------
/*
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

void oled_init()
{
    oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    oled.setTextColor(WHITE);
    oled.setTextSize(2);
    oled.setRotation(0);
    oled.clearDisplay();
    oled.display();
}
*/
