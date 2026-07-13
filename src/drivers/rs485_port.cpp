#include "drivers/rs485_port.h"

// Dedicated UART instance for the RS485 / Modbus link.
// Constructing from the RX/TX pins lets the core bind the matching USART
// (RX=PB7, TX=PA9 -> USART1).
static HardwareSerial SerialRS485(HW_UART_RS485_RX_PIN, HW_UART_RS485_TX_PIN);
RS485Class rs485(SerialRS485, HW_UART_RS485_DE_PIN, HW_UART_RS485_TX_PIN, HW_UART_RS485_RX_PIN);

void rs485PortBegin(uint32_t baud)
{
    SerialRS485.begin(baud);
}
