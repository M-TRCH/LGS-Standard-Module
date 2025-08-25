
#ifndef EEPROM_UTILS_H
#define EEPROM_UTILS_H

#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"

// Example struct for all persistent config
typedef struct 
{
    uint16_t deviceType;
    uint16_t fwVersion;
    uint16_t serialNumber;
    uint16_t baudRate;
    uint16_t identifier;
    uint16_t led_brightness[8];
    uint16_t led_r[8];
    uint16_t led_g[8];
    uint16_t led_b[8];
} EepromConfig_t;

extern EepromConfig_t eepromConfig;
extern EepromConfig_t eepromConfig_cache;

// Load all config from EEPROM to RAM
void loadEepromConfig();

// Save all config from RAM to EEPROM (write only if changed)
void saveEepromConfig();

#endif