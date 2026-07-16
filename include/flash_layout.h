#ifndef FLASH_LAYOUT_H
#define FLASH_LAYOUT_H

#include <stdint.h>

/*  @file flash_layout.h
 *  @brief Flash partition map for OTA-over-RS485 (L0: pure constants).
 *
 *  STM32G070CB: 128KB, 64 x 2KB pages, single bank.
 *
 *    0x08000000  boot     2 pages   4KB   bare-metal bootloader (env LGS_BOOT)
 *    0x08001000  app     31 pages  62KB   application (flash_offset = 0x1000)
 *    0x08010800  staging 31 pages  62KB   header page + image area (30 pages)
 *
 *  OTA flow: the app downloads the new image into the staging IMAGE area via
 *  Modbus broadcast chunks, verifies its CRC32, then commits the staging
 *  HEADER as the final step and resets. The bootloader copies a valid staged
 *  image into the app slot (idempotent: the header is erased only after the
 *  copy verifies, so a power loss mid-copy just re-copies on the next boot).
 *
 *  Shared by the app (drivers/flash_stage, app/ota_control) and the
 *  bootloader (src/boot) — this is the single source of truth; any change
 *  here requires reflashing BOTH images by ST-Link.
 */

#define FLASH_LAYOUT_PAGE_SIZE      0x800u        // 2KB (G0)

#define FLASH_BOOT_ADDR             0x08000000u
#define FLASH_BOOT_PAGES            2u            // 4KB

#define FLASH_APP_ADDR              0x08001000u   // = board_build.flash_offset 0x1000
#define FLASH_APP_PAGES             31u           // 62KB slot
#define FLASH_APP_FIRST_PAGE        2u

#define FLASH_STAGING_HEADER_ADDR   0x08010800u   // 1 page: the commit header
#define FLASH_STAGING_HEADER_PAGE   33u
#define FLASH_STAGING_IMAGE_ADDR    0x08011000u   // 30 pages: the downloaded image
#define FLASH_STAGING_IMAGE_PAGES   30u
#define FLASH_STAGING_IMAGE_FIRST_PAGE 34u

// Maximum OTA image = the staging image area. board_upload.maximum_size in
// platformio.ini caps the app link at the same number, so an image too big
// to transfer fails at build time, never in the field.
#define FLASH_OTA_MAX_IMAGE_SIZE    (FLASH_STAGING_IMAGE_PAGES * FLASH_LAYOUT_PAGE_SIZE) // 61,440

// OTA transfer chunk: 128 bytes. Sized so one Modbus FC16 broadcast frame
// (68 registers, ADU 145 bytes) fits the enlarged 256-byte UART RX ring,
// and so chunks divide both the flash page and the image cap exactly.
#define FLASH_OTA_CHUNK_SIZE        128u
#define FLASH_OTA_MAX_CHUNKS        (FLASH_OTA_MAX_IMAGE_SIZE / FLASH_OTA_CHUNK_SIZE)    // 480

// Staging header (written LAST, after the staged image verifies).
// hdrCrc16 = CRC16-CCITT over magic..crc32 so a torn header self-invalidates.
#define FLASH_OTA_HEADER_MAGIC      0x4C475355u   // 'LGSU'

typedef struct
{
    uint32_t magic;      // FLASH_OTA_HEADER_MAGIC
    uint32_t imageSize;  // bytes, 1..FLASH_OTA_MAX_IMAGE_SIZE
    uint32_t imageCrc32; // CRC-32/ISO-HDLC (zlib) over the staged image
    uint16_t hdrCrc16;   // CRC16-CCITT over the 12 bytes above
    uint16_t reserved;   // keeps the struct doubleword-friendly (16 bytes)
} OtaStagingHeader;

// --- Layout invariants ---
#ifdef __cplusplus
static_assert(FLASH_BOOT_ADDR + FLASH_BOOT_PAGES * FLASH_LAYOUT_PAGE_SIZE == FLASH_APP_ADDR,
              "app follows boot");
static_assert(FLASH_APP_ADDR + FLASH_APP_PAGES * FLASH_LAYOUT_PAGE_SIZE == FLASH_STAGING_HEADER_ADDR,
              "staging header follows app");
static_assert(FLASH_STAGING_HEADER_ADDR + FLASH_LAYOUT_PAGE_SIZE == FLASH_STAGING_IMAGE_ADDR,
              "image area follows the header page");
static_assert(FLASH_STAGING_IMAGE_ADDR + FLASH_STAGING_IMAGE_PAGES * FLASH_LAYOUT_PAGE_SIZE
                  == 0x08000000u + 128u * 1024u,
              "layout fills the 128KB part exactly");
static_assert(FLASH_OTA_MAX_IMAGE_SIZE <= FLASH_APP_PAGES * FLASH_LAYOUT_PAGE_SIZE,
              "a staged image always fits the app slot");
static_assert(sizeof(OtaStagingHeader) == 16, "header layout frozen");
#endif

#endif // FLASH_LAYOUT_H
