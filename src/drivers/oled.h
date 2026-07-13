#ifndef DRIVERS_OLED_H
#define DRIVERS_OLED_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "board.h"

/*  @file drivers/oled.h
 *  @brief 0.96" SSD1306 OLED driver on the dedicated I2C2 bus.
 */

#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_I2C_ADDR   0x3C

/*  @brief Initialize the I2C2 bus and the SSD1306 panel.
 *  @return true if the display was found and initialized
 */
bool oledInit();

/*  @brief Clear the display buffer and push it to the panel. */
void oledClear();

/*  @brief Show a single string, replacing the current content.
 *  @param text     Text to display
 *  @param textSize Font scale (default 2)
 */
void oledPrint(const String &text, uint8_t textSize = 2);

/*  @brief Show a two-digit number centered using a large GFX font.
 *
 *  @param value Number to display. Values above 99 are wrapped by the caller.
 */
void oledPrintLargeNumber(uint8_t value);

#endif // DRIVERS_OLED_H
