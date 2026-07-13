#include "hw/serial.h"

// Dedicated UART instance for the RS485 / Modbus link.
// Constructing from the RX/TX pins lets the core bind the matching USART
// (RX=PB7, TX=PA9 -> USART1).
HardwareSerial SerialRS485(RX_PIN, TX_PIN);
RS485Class rs485(SerialRS485, DUMMY_PIN, TX_PIN, RX_PIN);

void serialInit()
{
    // RS485 / Modbus UART
    SerialRS485.begin(MODBUS_BAUD);
}
