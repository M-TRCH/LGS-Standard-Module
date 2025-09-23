
#include "eeprom_utils.h"

EepromConfig_t eepromConfig;
EepromConfig_t eepromConfig_cache;
EepromConfig_t eepromConfig_default = 
{
    .isFirstBootExceptID = false,               // Indicates if first boot except ID
    .isFirstBoot = false,                       // Indicates if first boot
    .deviceType = DEFAULT_DEVICE_TYPE,          // Default device type
    .fwVersion = DEFAULT_FW_VERSION,            // Default firmware version 1.00
    .hwVersion = DEFAULT_HW_VERSION,            // Default hardware version
    .baudRate = DEFAULT_BAUD_RATE,              // Default baud rate
    .identifier = DEFAULT_IDENTIFIER,           // Default Modbus identifier
    .led_brightness = DEFAULT_LED_BRIGHTNESS,   // Default brightness for 8 LEDs
    .led_r = DEFAULT_LED_R,                     // Default red values for 8 LEDs
    .led_g = DEFAULT_LED_G,                     // Default green values for 8 LEDs
    .led_b = DEFAULT_LED_B,                     // Default blue values for 8 LEDs
    .maxOnTime = DEFAULT_LED_MAX_ON_TIME        // Default max on time for 8 LEDs
};

void loadEepromConfig() 
{
    EEPROM.get(0, eepromConfig);
    eepromConfig_cache = eepromConfig; // keep a copy for change detection
}

void clearEeprom(bool whileRunning) 
{
    EEPROM.begin();
    for (int i = 0; i < sizeof(EepromConfig_t); i++) 
    {
        EEPROM.write(i, 0xFF);
    }
    EEPROM.end();
    PRINT(DEBUG_BASIC, F("EEPROM cleared\n"));
    while (whileRunning)
    {
        PRINT(DEBUG_BASIC, F(".\n"));
        delay(3000);
    }
}

bool saveEepromConfig() 
{
    // Only write if data changed
    if (memcmp(&eepromConfig, &eepromConfig_cache, sizeof(EepromConfig_t)) != 0) 
    {
        EEPROM.put(0, eepromConfig);
        eepromConfig_cache = eepromConfig; // update cache
        PRINT(DEBUG_BASIC, F("EEPROM saved\n"));
        return true;
    }
    PRINT(DEBUG_BASIC, F("No EEPROM update needed\n"));
    return false;
}

void handleFirstBoot() 
{
    loadEepromConfig(); // Load configuration from EEPROM

    // write default values to EEPROM
    if (eepromConfig.isFirstBoot == true) 
    {
        PRINT(DEBUG_BASIC, F("First time programming\n"));
        eepromConfig = eepromConfig_default;
        saveEepromConfig();                             // Save updated config to EEPROM
        NVIC_SystemReset();                             // Perform software reset
    }

    // write default values to EEPROM except ID
    if (eepromConfig.isFirstBootExceptID == true) 
    {
        PRINT(DEBUG_BASIC, F("First time programming (Except ID)\n"));
        uint16_t saved_id = eepromConfig.identifier;    // save current ID
        eepromConfig = eepromConfig_default;
        eepromConfig.identifier = saved_id;             // restore ID
        saveEepromConfig();                             // Save updated config to EEPROM
        NVIC_SystemReset();                             // Perform software reset
    }
}