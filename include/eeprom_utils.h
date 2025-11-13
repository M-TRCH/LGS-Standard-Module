
#ifndef EEPROM_UTILS_H
#define EEPROM_UTILS_H

#include <Arduino.h>
#include <EEPROM.h>
#include "system.h"

// Example struct for all persistent config
typedef struct 
{
    bool isFirstBootExceptID;
    bool isFirstBoot;
    uint16_t deviceType;
    uint16_t fwVersion;
    uint16_t hwVersion;
    uint16_t baudRate;
    uint16_t identifier;
    uint16_t led_brightness[8];
    uint16_t led_r[8];
    uint16_t led_g[8];
    uint16_t led_b[8];
    uint16_t maxOnTime[8];
    uint16_t unlockDelayTime;
} EepromConfig_t;

extern EepromConfig_t eepromConfig;
extern EepromConfig_t eepromConfig_cache;
extern EepromConfig_t eepromConfig_default;

/*  @brief Initialize EEPROM and load configuration
*/
void eepromInit();

/*  @brief Clear entire EEPROM (set all bytes to 0xFF)
    @param whileRunning If true, will enter infinite loop after clearing (for debugging)
*/
void clearEeprom(bool whileRunning=false);

// Load all config from EEPROM to RAM
void loadEepromConfig();

/*  @brief Save all config from RAM to EEPROM
    @return true if saved successfully, false otherwise
*/
bool saveEepromConfig();

#endif