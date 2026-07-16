#include "drivers/rs485_port.h"

// Dedicated UART instance for the RS485 / Modbus link.
// Constructing from the RX/TX pins lets the core bind the matching USART
// (RX=PB7, TX=PA9 -> USART1).
static HardwareSerial SerialRS485(HW_UART_RS485_RX_PIN, HW_UART_RS485_TX_PIN);

// The transceiver (COSMAX13487, a MAX13487E-class part) is AUTO-DIRECTION: its
// driver enables itself on TX activity and RE is hardwired (tied to 3V3). There
// is no MCU-controlled DE/RE line, so both are passed as -1. Handing real pins
// here would make RS485Class::begin() drive them as GPIO before the UART
// reclaims them — and the only pins available were the UART's own PA9/PB7, so
// it was clobbering the very link it drives. With -1/-1 the direction calls are
// no-ops and the UART owns PA9/PB7 outright.
RS485Class rs485(SerialRS485, HW_UART_RS485_TX_PIN, -1, -1);

void rs485PortBegin(uint32_t baud)
{
    SerialRS485.begin(baud);
}
