
#include "eeprom_utils.h"

EepromConfig_t eepromConfig;
EepromConfig_t eepromConfig_cache;
EepromConfig_t eepromConfig_default = 
{
    .isFirstBoot = false,                       // Indicates if first boot
    .deviceType = DEFAULT_DEVICE_TYPE,          // Default device type
    .fwVersion = DEFAULT_FW_VERSION,            // Default firmware version 1.00
    .serialNumber = DEFAULT_SERIAL_NUMBER,      // Default serial number
    .baudRate = DEFAULT_BAUD_RATE,              // Default baud rate
    .identifier = DEFAULT_IDENTIFIER,           // Default Modbus identifier
    .led_brightness = DEFAULT_LED_BRIGHTNESS,   // Default brightness for 8 LEDs
    .led_r = DEFAULT_LED_R,                     // Default red values for 8 LEDs
    .led_g = DEFAULT_LED_G,                     // Default green values for 8 LEDs
    .led_b = DEFAULT_LED_B                      // Default blue values for 8 LEDs
};

void loadEepromConfig() 
{
    EEPROM.get(0, eepromConfig);
    eepromConfig_cache = eepromConfig; // keep a copy for change detection
}

void clearEeprom() 
{
    EEPROM.begin();
    for (int i = 0; i < sizeof(EepromConfig_t); i++) 
    {
        EEPROM.write(i, 0xFF);
    }
    EEPROM.end();
}

void saveEepromConfig() 
{
    // Only write if data changed
    if (memcmp(&eepromConfig, &eepromConfig_cache, sizeof(EepromConfig_t)) != 0) 
    {
        EEPROM.put(0, eepromConfig);
        eepromConfig_cache = eepromConfig; // update cache
        Serial.println("EEPROM configuration saved");
    }
}