#ifndef OTA_H
#define OTA_H

#include <Arduino.h>

// Flash memory layout for dual-bank OTA
#define OTA_APP_ADDR         0x08000000U   // Application area (first 32KB)
#define OTA_STAGING_ADDR     0x08008000U   // Staging area (second 32KB)
#define OTA_MAX_FW_SIZE      0x8000U       // 32KB max firmware size
#define OTA_FLASH_PAGE_SIZE  0x400U        // 1KB per page (STM32F103 medium-density)

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
