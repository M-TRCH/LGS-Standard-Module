
#include "modbus_utils.h"
#include "hw/serial.h"

// Global variables for Modbus
uint16_t last_global_brightness = 0;
uint16_t last_global_max_on_time = 0;

// Modbus RTU Server Object
ModbusRTUServerClass RTUServer;

void modbusInit(int id) 
{
    id = abs(id);

    RTUServer.begin(rs485, id, MODBUS_BAUD, SERIAL_8N1);

    RTUServer.configureCoils(0x00, COIL_NUM);
    RTUServer.configureDiscreteInputs(0x00, DISCRETE_INPUT_NUM);
    RTUServer.configureHoldingRegisters(0x00, HOLDING_REGISTER_NUM);
    RTUServer.configureInputRegisters(0x00, INPUT_REGISTER_NUM);
}

void modbus2eepromMapping(bool saveEEPROM) 
{
    // Write Modbus values to EEPROM
    // Device group:
    eepromConfig.baudRate = RTUServer.holdingRegisterRead(MB_REG_BAUD_RATE);
    eepromConfig.identifier = RTUServer.holdingRegisterRead(MB_REG_IDENTIFIER);

    // Configuration group:
    // LED Configuration
    for (int i = 0; i < LED_NUM; i++) 
    {
        // led_brightness, led_r, led_g, led_b
        eepromConfig.led_brightness[i] = RTUServer.holdingRegisterRead(MB_REG_LED_1_BRIGHTNESS + i*10);
        eepromConfig.led_r[i] = RTUServer.holdingRegisterRead(MB_REG_LED_1_RED + i*10);
        eepromConfig.led_g[i] = RTUServer.holdingRegisterRead(MB_REG_LED_1_GREEN + i*10);
        eepromConfig.led_b[i] = RTUServer.holdingRegisterRead(MB_REG_LED_1_BLUE + i*10);

        // maximum on-time limit
        eepromConfig.maxOnTime[i] = RTUServer.holdingRegisterRead(MB_REG_LED_1_MAX_ON_TIME + i*10);
    }
    
    // Unlock delay time
    eepromConfig.unlockDelayTime = RTUServer.holdingRegisterRead(MB_REG_UNLOCK_DELAY);

    // Number of LEDs per strip
    eepromConfig.ledNumPerStrip = RTUServer.holdingRegisterRead(MB_REG_LED_NUM_PER_STRIP);

    if (saveEEPROM) 
    {
        saveEepromConfig(); // Save to EEPROM if changed
    } 
}

void eeprom2modbusMapping(bool loadEEPROM) 
{
    if (loadEEPROM) 
    {
        loadEepromConfig(); // Load configuration from EEPROM
    }

    // Update Modbus registers with EEPROM 
    // Device group:
    RTUServer.holdingRegisterWrite(MB_REG_DEVICE_TYPE, eepromConfig.deviceType);
    RTUServer.holdingRegisterWrite(MB_REG_FW_VERSION, eepromConfig.fwVersion);
    RTUServer.holdingRegisterWrite(MB_REG_HW_VERSION, eepromConfig.hwVersion);
    RTUServer.holdingRegisterWrite(MB_REG_BAUD_RATE, eepromConfig.baudRate);
    RTUServer.holdingRegisterWrite(MB_REG_IDENTIFIER, eepromConfig.identifier);

    // Configuration group:
    // LED Configuration
    for (int i = 0; i < LED_NUM; i++) 
    {
        // led_brightness, led_r, led_g, led_b
        RTUServer.holdingRegisterWrite(MB_REG_LED_1_BRIGHTNESS + i*10, eepromConfig.led_brightness[i]);
        RTUServer.holdingRegisterWrite(MB_REG_LED_1_RED + i*10, eepromConfig.led_r[i]);
        RTUServer.holdingRegisterWrite(MB_REG_LED_1_GREEN + i*10, eepromConfig.led_g[i]);
        RTUServer.holdingRegisterWrite(MB_REG_LED_1_BLUE + i*10, eepromConfig.led_b[i]);
    
        // maximum on-time limit
        RTUServer.holdingRegisterWrite(MB_REG_LED_1_MAX_ON_TIME + i*10, eepromConfig.maxOnTime[i]);
    }

    // Unlock delay time
    RTUServer.holdingRegisterWrite(MB_REG_UNLOCK_DELAY, eepromConfig.unlockDelayTime);

    // Number of LEDs per strip
    RTUServer.holdingRegisterWrite(MB_REG_LED_NUM_PER_STRIP, eepromConfig.ledNumPerStrip);
}
