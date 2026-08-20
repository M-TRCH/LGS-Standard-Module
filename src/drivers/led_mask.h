#ifndef DRIVERS_LED_MASK_H
#define DRIVERS_LED_MASK_H

#include <Arduino.h>
#include "board.h"

/*  @file drivers/led_mask.h
 *  @brief LED-8-Index mask: 8 SK6812MINI RGBW pixels chained on PB5.
 *
 *  An accessory worn over the OLED position on Standard-type cabinets.
 *  It mirrors the active preset — preset n lights index n in the preset's
 *  color — so it adds nothing to the Modbus surface. Driving the pin with
 *  no mask fitted is harmless; PB5 is the expansion pad reserved for
 *  exactly this part (see board.h).
 */

/*  @brief Initialize the mask chain (data pin low, all pixels dark). */
void maskInit();

/*  @brief Light index 1-8 in @p color, all other pixels dark.
 *         Out-of-range indexes turn the whole mask off. */
void maskShowIndex(uint8_t index, uint32_t color);

/*  @brief All 8 pixels to one color (identify/acknowledge overlays). */
void maskSetAll(uint32_t color);

/*  @brief All pixels dark. */
void maskOff();

/*  @brief One frame of the demo rainbow, @p phase advancing per frame.
 *
 *  The mask's counterpart to the ring's ledShowRainbowRipple: same hue
 *  sweep and same brightness wave, spread over 8 windows instead of 16,
 *  so a Standard cabinet in DEMO looks like the same product as a ring
 *  one. The ripple travels in READING order, not chain order — a demo is
 *  something a person watches. */
void maskShowRainbow(uint16_t phase);

#endif // DRIVERS_LED_MASK_H
