
#include <Arduino.h>
#include "config.h"
#include "system.h"
#include "led.h"
#include "modbus_utils.h"
#include "eeprom_utils.h"

uint8_t last_led_state[8] = {0};

void setup() 
{
    #ifdef SYSTEM_H
        sysInit();  // Initialize system
    #endif

    #ifdef LED_H
        ledInit();  // Initialize LEDs
    #endif

    #ifdef MODBUS_UTILS_H
        modbusInit();  // Initialize Modbus
    #endif

    loadEepromConfig(); // Load configuration from EEPROM
    eepromConfig_cache = eepromConfig;

    // eepromConfig.serialNumber = 3456; // Set a default serial number
    // saveEepromConfig(); // Save to EEPROM if changed
}

void loop() 
{   
    RTUServer.holdingRegisterWrite(MB_REG_SERIAL_NUMBER, eepromConfig.serialNumber);

    if(RTUServer.poll()) 
    {
        // read the current value of the coil
        for (int i = 0; i < 8; i++) 
        {
            int led_state = RTUServer.coilRead(0x00 + i);
            if (led_state != last_led_state[i]) // Check if the LED state has changed
            {
                last_led_state[i] = led_state;

                if (led_state) // If the LED state is ON
                {
                    leds[i]->setPixelColor(0, leds[i]->Color(default_color[i][0], default_color[i][1], default_color[i][2]));
                    leds[i]->show();
                }
                else
                {
                    leds[i]->setPixelColor(0, leds[i]->Color(0, 0, 0));
                    leds[i]->show();
                }
            }
        }
    }
}
