
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
}

void modbus2eepromMapping() 
{
    // Write Modbus values to EEPROM
    eepromConfig.baudRate = RTUServer.holdingRegisterRead(MB_REG_BAUD_RATE);
    eepromConfig.identifier = RTUServer.holdingRegisterRead(MB_REG_IDENTIFIER);
}

void eeprom2modbusMapping() 
{
    // Read EEPROM values and update Modbus
    RTUServer.holdingRegisterWrite(MB_REG_DEVICE_TYPE, eepromConfig.deviceType);
    RTUServer.holdingRegisterWrite(MB_REG_FW_VERSION, eepromConfig.fwVersion);
    RTUServer.holdingRegisterWrite(MB_REG_SERIAL_NUMBER, eepromConfig.serialNumber);
    RTUServer.holdingRegisterWrite(MB_REG_BAUD_RATE, eepromConfig.baudRate);
    RTUServer.holdingRegisterWrite(MB_REG_IDENTIFIER, eepromConfig.identifier);
}
