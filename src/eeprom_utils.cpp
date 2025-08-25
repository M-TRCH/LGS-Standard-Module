#include "eeprom_utils.h"

EepromConfig_t eepromConfig;
EepromConfig_t eepromConfig_cache;

void loadEepromConfig() 
{
    EEPROM.get(0, eepromConfig);
    eepromConfig_cache = eepromConfig; // keep a copy for change detection
}

void saveEepromConfig() 
{
    // Only write if data changed
    if (memcmp(&eepromConfig, &eepromConfig_cache, sizeof(EepromConfig_t)) != 0) 
    {
        EEPROM.put(0, eepromConfig);
        eepromConfig_cache = eepromConfig; // update cache
    }
}