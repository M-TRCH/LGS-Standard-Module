
#include "modbus_utils.h"

ModbusRTUServerClass RTUServer;

void modbusInit(int id) 
{
    id = abs(id);

    #if MODBUS_OUTPUT == MODBUS_SERIAL
        RTUServer.begin(rs485, id, MODBUS_BAUD, SERIAL_8N1);   // Start Modbus RTU server with ID 1, baud rate 9600, and config SERIAL_8N1
    #elif MODBUS_OUTPUT == MODBUS_SERIAL3
        RTUServer.begin(rs4853, id, MODBUS_BAUD, SERIAL_8N1);  // Start Modbus RTU server with Serial3
    #endif

    RTUServer.configureCoils(0x00, COIL_NUM);
    RTUServer.configureDiscreteInputs(0x00, DISCRETE_INPUT_NUM);
    RTUServer.configureHoldingRegisters(0x00, HOLDING_REGISTER_NUM);
    RTUServer.configureInputRegisters(0x00, INPUT_REGISTER_NUM);

    LOG_INFO_MODBUS("[MODBUS] Modbus RTU server started with ID: " + String(id) + "\n");
}

void modbus2eepromMapping(bool saveEEPROM) 
{
    // Write Modbus values to EEPROM
    // Device Information
    eepromConfig.baudRate = RTUServer.holdingRegisterRead(MB_REG_BAUD_RATE);
    eepromConfig.identifier = RTUServer.holdingRegisterRead(MB_REG_IDENTIFIER);

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
    // Device Information
    RTUServer.holdingRegisterWrite(MB_REG_DEVICE_TYPE, eepromConfig.deviceType);
    RTUServer.holdingRegisterWrite(MB_REG_FW_VERSION, eepromConfig.fwVersion);
    RTUServer.holdingRegisterWrite(MB_REG_HW_VERSION, eepromConfig.hwVersion);
    RTUServer.holdingRegisterWrite(MB_REG_BAUD_RATE, eepromConfig.baudRate);
    RTUServer.holdingRegisterWrite(MB_REG_IDENTIFIER, eepromConfig.identifier);

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
    
    LOG_INFO_MODBUS(F("[MODBUS] EEPROM to Modbus mapping applied\n"));
}
