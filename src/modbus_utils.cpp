
#include "modbus_utils.h"
#include "drivers/rs485_port.h"
#include "version.h"

// Global variables for Modbus
uint16_t last_global_brightness = 0;
uint16_t last_global_max_on_time = 0;

// Modbus RTU Server Object
ModbusRTUServerClass RTUServer;

void modbusInit(int id) 
{
    id = abs(id);

    RTUServer.begin(rs485, id, DEFAULT_BAUD_RATE, SERIAL_8N1);

    RTUServer.configureCoils(0x00, COIL_NUM);
    RTUServer.configureDiscreteInputs(0x00, DISCRETE_INPUT_NUM);
    RTUServer.configureHoldingRegisters(0x00, HOLDING_REGISTER_NUM);
    RTUServer.configureInputRegisters(0x00, INPUT_REGISTER_NUM);
}

void modbus2eepromMapping(bool saveEEPROM)
{
    Settings &s = settingsEdit();

    // Device group:
    s.baudRate   = RTUServer.holdingRegisterRead(MB_REG_BAUD_RATE);
    s.identifier = RTUServer.holdingRegisterRead(MB_REG_IDENTIFIER);

    // Configuration group:
    s.ledBrightness = RTUServer.holdingRegisterRead(MB_REG_LED_1_BRIGHTNESS);
    s.ledR          = RTUServer.holdingRegisterRead(MB_REG_LED_1_RED);
    s.ledG          = RTUServer.holdingRegisterRead(MB_REG_LED_1_GREEN);
    s.ledB          = RTUServer.holdingRegisterRead(MB_REG_LED_1_BLUE);
    s.ledMaxOnTimeS = RTUServer.holdingRegisterRead(MB_REG_LED_1_MAX_ON_TIME);
    s.unlockDelayMs = RTUServer.holdingRegisterRead(MB_REG_UNLOCK_DELAY);

    if (saveEEPROM)
    {
        settingsSave(); // Persist to the AT24 if changed
    }
}

void eeprom2modbusMapping()
{
    const Settings &s = settings();

    // Device group: identity registers come from compile-time constants so
    // they always reflect the running build (never stale EEPROM copies).
    RTUServer.holdingRegisterWrite(MB_REG_DEVICE_TYPE, DEVICE_TYPE);
    RTUServer.holdingRegisterWrite(MB_REG_FW_VERSION, FW_VERSION);
    RTUServer.holdingRegisterWrite(MB_REG_HW_VERSION, HW_VERSION);
    RTUServer.holdingRegisterWrite(MB_REG_BAUD_RATE, s.baudRate);
    RTUServer.holdingRegisterWrite(MB_REG_IDENTIFIER, s.identifier);

    // Configuration group:
    RTUServer.holdingRegisterWrite(MB_REG_LED_1_BRIGHTNESS, s.ledBrightness);
    RTUServer.holdingRegisterWrite(MB_REG_LED_1_RED, s.ledR);
    RTUServer.holdingRegisterWrite(MB_REG_LED_1_GREEN, s.ledG);
    RTUServer.holdingRegisterWrite(MB_REG_LED_1_BLUE, s.ledB);
    RTUServer.holdingRegisterWrite(MB_REG_LED_1_MAX_ON_TIME, s.ledMaxOnTimeS);
    RTUServer.holdingRegisterWrite(MB_REG_UNLOCK_DELAY, s.unlockDelayMs);
}
