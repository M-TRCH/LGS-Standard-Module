
#include "modbus_utils.h"

// Global variables for Modbus
uint16_t last_global_brightness = 0;
uint16_t last_global_max_on_time = 0;

// ===== Modbus RTU Server (Commented Out) =====
/*
// Modbus RTU Server Object
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
    
    LOG_INFO_MODBUS(F("[MODBUS] EEPROM to Modbus mapping applied\n"));
}
*/

// ===== Modbus RTU Client Functions =====

// Modbus RTU Client Object
ModbusRTUClientClass RTUClient;

void modbusClientInit() 
{
    #if MODBUS_OUTPUT == MODBUS_SERIAL
        RTUClient.begin(rs485, MODBUS_BAUD, SERIAL_8N1);
        LOG_INFO_MODBUS(F("[MODBUS] Modbus RTU Client initialized on Serial (RS485)\n"));
    #elif MODBUS_OUTPUT == MODBUS_SERIAL3
        RTUClient.begin(rs4853, MODBUS_BAUD, SERIAL_8N1);
        LOG_INFO_MODBUS(F("[MODBUS] Modbus RTU Client initialized on Serial3 (RS485)\n"));
    #endif
    
    LOG_INFO_MODBUS(F("[MODBUS] Modbus RTU Client ready for broadcast\n"));
}

void broadcastOperate(bool ledStates[8]) 
{
    LOG_INFO_MODBUS(F("[MODBUS] Broadcasting LED states: ["));
    for (int i = 0; i < 8; i++) 
    {
        LOG_INFO_MODBUS(String(ledStates[i] ? "1" : "0"));
        if (i < 7) LOG_INFO_MODBUS(F(","));
    }
    LOG_INFO_MODBUS(F("]\n"));
    
    // Write multiple coils using broadcast address (0)
    // Address 1001-1008 for LED 1-8 Enable
    RTUClient.beginTransmission(MODBUS_BROADCAST_ID, COILS, MODBUS_COIL_START_ADDR, MODBUS_COIL_COUNT);
    
    for (int i = 0; i < MODBUS_COIL_COUNT; i++) 
    {
        RTUClient.write(ledStates[i]);
    }
    
    int result = RTUClient.endTransmission();
    
    if (result == 1) 
    {
        LOG_INFO_MODBUS(F("[MODBUS] Broadcast successful\n"));
    } 
    else 
    {
        LOG_INFO_MODBUS(F("[MODBUS] Broadcast failed with error code: "));
        LOG_INFO_MODBUS(String(result) + "\n");
    }
}

void broadcastSingleLed(uint8_t ledNumber, bool state) 
{
    if (ledNumber < 1 || ledNumber > 8) 
    {
        LOG_INFO_MODBUS(F("[MODBUS] Invalid LED number. Must be 1-8\n"));
        return;
    }
    
    LOG_INFO_MODBUS(F("[MODBUS] Broadcasting LED"));
    LOG_INFO_MODBUS(String(ledNumber));
    LOG_INFO_MODBUS(F(" = "));
    LOG_INFO_MODBUS(String(state ? "ON" : "OFF"));
    LOG_INFO_MODBUS(F("\n"));
    
    // Calculate the coil address for the specific LED
    uint16_t coilAddress = MODBUS_COIL_START_ADDR + (ledNumber - 1);
    
    // Write single coil using broadcast address (0)
    int result = RTUClient.coilWrite(MODBUS_BROADCAST_ID, coilAddress, state);
    
    if (result == 1) 
    {
        LOG_INFO_MODBUS(F("[MODBUS] Broadcast successful\n"));
    } 
    else 
    {
        LOG_INFO_MODBUS(F("[MODBUS] Broadcast failed with error code: "));
        LOG_INFO_MODBUS(String(result) + "\n");
    }
}

void broadcastAllLeds(bool state) 
{
    bool ledStates[8];
    
    // Set all LEDs to the same state
    for (int i = 0; i < 8; i++) 
    {
        ledStates[i] = state;
    }
    
    LOG_INFO_MODBUS(F("[MODBUS] Broadcasting ALL LEDs = "));
    LOG_INFO_MODBUS(String(state ? "ON" : "OFF"));
    LOG_INFO_MODBUS(F("\n"));
    
    broadcastOperate(ledStates);
}
