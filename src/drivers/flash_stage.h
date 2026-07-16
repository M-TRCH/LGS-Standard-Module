#ifndef DRIVERS_FLASH_STAGE_H
#define DRIVERS_FLASH_STAGE_H

#include <Arduino.h>
#include "flash_layout.h"

/*  @file drivers/flash_stage.h
 *  @brief OTA staging-area flash driver (HAL): erase, chunk writes, CRC32
 *         and the final header commit. Layout comes from flash_layout.h.
 *
 *  Writes are 8-byte (doubleword) HAL programs; a partial trailing chunk is
 *  0xFF-padded. The caller (app/ota_control) is responsible for never
 *  writing the same chunk twice — reprogramming a non-blank doubleword
 *  raises PROGERR on the G0.
 */

/*  @brief Erase the whole staging area (header page + 30 image pages).
 *         ~700ms of bus-stalling page erases; the watchdog is reloaded
 *         around every page. Call once when an OTA session starts. */
void flashStageEraseAll();

/*  @brief Program one received chunk into the staging image area.
 *  @param offset byte offset inside the image (chunkIndex * 128; 8-aligned)
 *  @param data   payload bytes
 *  @param len    1..FLASH_OTA_CHUNK_SIZE; padded with 0xFF to doublewords
 *  @return false on a HAL programming error */
bool flashStageWriteChunk(uint32_t offset, const uint8_t *data, uint16_t len);

/*  @brief CRC-32/ISO-HDLC (zlib) over the first @p size staged image bytes. */
uint32_t flashStageCrc32(uint32_t size);

/*  @brief Commit the staging header — THE point of no return: after this,
 *         the bootloader applies the staged image on the next reset.
 *         Only call after flashStageCrc32 matched the expected image CRC.
 *  @return false on a HAL programming error */
bool flashStageCommitHeader(uint32_t imageSize, uint32_t imageCrc32);

#endif // DRIVERS_FLASH_STAGE_H
