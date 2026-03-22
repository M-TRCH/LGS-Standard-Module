
#include <Arduino.h>
#include "led.h"
#include "eeprom_utils.h"
#include "ota.h"

void setup() 
{
#ifdef SYSTEM_H
    sysInit(LOG_INFO);  // Initialize system
#endif

#ifdef EEPROM_UTILS_H
    // clearEeprom(true);   // Uncomment to clear EEPROM for debugging  
    eepromInit();           // Initialize EEPROM and load configuration
    printEepromConfig();    // Print loaded configuration for verification
#endif

#ifdef LED_H
    ledInit();  // Initialize LEDs
#endif

    // Enter OTA firmware update mode if FUNC_SW held >= 3s at boot
    if (functionMode == FUNC_SW_OTA)
    {
        Serial.println(F("[MAIN] Entering OTA mode..."));
        bool otaOk = otaReceiveFirmware(Serial3);
        if (!otaOk)
        {
            Serial.println(F("[MAIN] OTA failed or timed out — resuming normal operation"));
        }
        // If OTA succeeded, MCU has already reset — won't reach here
    }
    else
    {
        Serial.println(F("[MAIN] Normal operation mode"));
    }
}

void loop() 
{   
    // Routine blink for run LED
    if (ON_ROUTINE_BLINK_RUN())
    {
        digitalWrite(LED_RUN_PIN, blink_run_state);
    }
}

