#ifndef OTA_H
#define OTA_H

#include <Arduino.h>

// Flash memory layout for dual-bank OTA
// ┌─────────────────────────────────────────────────────────┐
// │ 0x08000000 - 0x08007BFF : App firmware    (31KB max)   │
// │ 0x08007C00 - 0x08007FFF : Persistent config (1KB)      │
// │ 0x08008000 - 0x0800FBFF : OTA staging area (31KB max)  │
// │ 0x0800FC00 - 0x0800FFFF : EEPROM emulation (1KB)       │
// └─────────────────────────────────────────────────────────┘
#define OTA_APP_ADDR         0x08000000U   // Application area (first 32KB bank)
#define OTA_STAGING_ADDR     0x08008000U   // Staging area (second 32KB bank)
#define OTA_MAX_FW_SIZE      0x7C00U       // 31KB max firmware size (protects persistent & EEPROM pages)
#define OTA_FLASH_PAGE_SIZE  0x400U        // 1KB per page (STM32F103 medium-density)

// Persistent config — survives OTA firmware updates
#define OTA_PERSIST_ADDR     0x08007C00U   // Last page of app bank (1KB)
#define OTA_PERSIST_MAGIC    0x4F544131U   // 'OTA1' magic marker

// OTA protocol configuration
#define OTA_PACKET_DATA_SIZE 128U          // Max data bytes per DATA packet
#define OTA_START_TIMEOUT_MS 30000U        // Timeout waiting for START (30s)
#define OTA_DATA_TIMEOUT_MS  5000U         // Timeout per DATA packet (5s)

// Protocol frame markers
#define OTA_SYNC_1           0xAA
#define OTA_SYNC_2           0x55

// Protocol commands (PC -> MCU broadcast only, no response)
#define OTA_CMD_START        0x01          // PC->MCU: firmware size (4 bytes LE)
#define OTA_CMD_DATA         0x02          // PC->MCU: seq(2) + data(N)
#define OTA_CMD_END          0x03          // PC->MCU: CRC32 (4 bytes LE)

// Persistent config struct — stored at OTA_PERSIST_ADDR
typedef struct {
    uint32_t magic;           // OTA_PERSIST_MAGIC if valid
    uint16_t identifier;      // Modbus Unit ID (1-247)
    uint16_t baudRate;        // Communication baud rate
    uint16_t checksum;        // XOR-16 checksum for validation
} OtaPersistConfig_t;

/**
 * Save critical config to persistent flash page (survives OTA).
 * Call before entering OTA mode.
 */
bool otaSavePersistentConfig(uint16_t identifier, uint16_t baudRate);

/**
 * Load critical config from persistent flash page.
 * Returns true if valid data found, false if page is blank/corrupt.
 */
bool otaLoadPersistentConfig(uint16_t *identifier, uint16_t *baudRate);

/**
 * Enter OTA receive mode via RS485/Serial (broadcast, receive-only).
 * MCU never transmits on the bus — supports multi-board simultaneous update.
 * Blocks until firmware is received, verified and applied (resets MCU),
 * or returns false on error.
 *
 * @param serial  Serial port for RS485 communication (typically Serial3)
 */
bool otaReceiveFirmware(HardwareSerial &serial);

#endif
