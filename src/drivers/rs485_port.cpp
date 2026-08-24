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

    // Bound how long one Modbus read may block. ArduinoModbus receives with
    // Stream::readBytes, which waits for the byte count it asked for or for
    // the stream timeout — and nothing had ever set that timeout, so it was
    // Arduino's 1000 ms default.
    //
    // That is fine for whole frames and disastrous for truncated ones: the
    // RS485 switch hub cuts a frame in half every time it changes channel,
    // and the modules on that channel then sat in readBytes across the
    // receive state machine's three steps (~3 s) plus libmodbus's own
    // recovery sleep — right at the 4 s watchdog. Measured on the bench
    // (2026-08-13): polling the whole cabinet cost every module on a
    // crossed channel ~0.6 watchdog resets per pass, while polling one
    // channel alone cost exactly none. The reboots were invisible to the
    // master (a module answers again within a second) but every lit slot
    // went dark, which is a pick disappearing under the pharmacist's hand.
    //
    // Since modbusServerTick() only polls after the RTU frame gap, every
    // byte of a complete frame is already in the ring when the library
    // reads — waiting for MORE bytes inside a read is never useful: bytes
    // that arrive later belong to the next frame.
    //
    // And the timeout must be tiny, not merely "a few characters", because
    // Stream::readBytes restarts it PER BYTE (timedRead resets _startMillis
    // on every call). That is the hole the 2026-08-13 fix left open: one
    // garbage byte parsed as a function code can make libmodbus ask for up
    // to ~250 bytes (the max-ADU guard is the only cap), and a marginal
    // line that dribbles noise bytes at intervals just under the timeout
    // keeps every one of those 250 reads alive — 250 x 29 ms = 7 s of stall
    // in ONE tick, sailing past the 4 s watchdog. That is the mechanism
    // behind whole-channel IWDG resets on a noisy branch: every module
    // hears the same dribble, each stalls past its own watchdog, and the
    // per-chip UART sampling decides the stable order in which they fall
    // (measured 2026-08-24: a marginal CH2 head contact reset all 32
    // modules of rows 6-10 about once a minute while that channel was
    // polled, and never once when only CH1 was driven).
    //
    // 2 ms covers interrupt/scheduling jitter for bytes already in flight
    // into the ring; the worst garbage parse is now 250 x 2 ms = 0.5 s,
    // bounded far below the watchdog. Line noise can still cost replies —
    // no_reply and slow rows — but never a CPU hang.
    (void)baud;
    rs485.setTimeout(2);
}
