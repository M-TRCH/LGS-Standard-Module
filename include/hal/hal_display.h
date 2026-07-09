#ifndef HAL_DISPLAY_H
#define HAL_DISPLAY_H

#include <Arduino.h>
#include "system.h"

/*
 * hal_display.h - Hardware abstraction for the optional OLED display
 * ------------------------------------------------------------------
 * Wraps the SSD1306 128x64 OLED. The display is optional hardware, so the
 * whole module is compiled out unless LGS_ENABLE_DISPLAY is set to 1 via a
 * PlatformIO build flag (-D LGS_ENABLE_DISPLAY=1). When disabled the functions
 * become no-ops, letting the application call them unconditionally.
 */

#ifndef LGS_ENABLE_DISPLAY
#define LGS_ENABLE_DISPLAY 0
#endif

/* @brief Initialize the OLED display (call once at startup). */
void displayInit();

/* @brief Turn the display content on or off. */
void displayEnable(bool enabled);

/* @brief Show an unsigned number centered on the display. */
void displayShowNumber(uint16_t value);

/* @brief Clear the display. */
void displayClear();

#endif // HAL_DISPLAY_H
