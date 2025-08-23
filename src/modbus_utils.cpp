
#include "modbus_utils.h"

ModbusRTUServerClass RTUServer;

void modbusInit() 
{
    #if MODBUS_OUTPUT == MODBUS_SERIAL
        RTUServer.begin(rs485, MODBUS_ID, MODBUS_BAUD, SERIAL_8N1);   // Start Modbus RTU server with ID 1, baud rate 9600, and config SERIAL_8N1
    #elif MODBUS_OUTPUT == MODBUS_SERIAL3
        RTUServer.begin(rs4853, MODBUS_ID, MODBUS_BAUD, SERIAL_8N1);  // Start Modbus RTU server with Serial3
    #endif

    RTUServer.configureCoils(0x00, COIL_NUM);
    RTUServer.configureDiscreteInputs(0x00, DISCRETE_INPUT_NUM);
    RTUServer.configureHoldingRegisters(0x00, HOLDING_REGISTER_NUM);
    RTUServer.configureInputRegisters(0x00, INPUT_REGISTER_NUM);
}
