
#include <Arduino.h>
#include "led.h"
#include "eeprom_utils.h"

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
}

void loop() 
{   
    // Routine blink for run LED
    if (ON_ROUTINE_BLINK_RUN())
    {
        digitalWrite(LED_RUN_PIN, blink_run_state);
    }
}

