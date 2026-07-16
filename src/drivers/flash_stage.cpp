#include "drivers/flash_stage.h"
#include <IWatchdog.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

// CRC-32/ISO-HDLC (zlib), bitwise — matches the host tool and the bootloader.
uint32_t crc32Bytes(const uint8_t *p, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    while (len--)
    {
        crc ^= *p++;
        for (int i = 0; i < 8; i++)
        {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

// CRC16-CCITT for the staging header (same polynomial as the bootloader).
uint16_t crc16Bytes(const uint8_t *p, uint32_t len)
{
    uint16_t crc = 0xFFFFu;
    while (len--)
    {
        crc ^= (uint16_t)(*p++) << 8;
        for (int i = 0; i < 8; i++)
        {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

bool programDoublewords(uint32_t addr, const uint8_t *data, uint32_t byteCount)
{
    // byteCount is already a multiple of 8 (callers pad).
    bool ok = true;
    HAL_FLASH_Unlock();
    FLASH->SR = FLASH->SR; // W1C: clear every latched error/status flag
    for (uint32_t i = 0; i < byteCount && ok; i += 8)
    {
        uint64_t dw;
        memcpy(&dw, data + i, sizeof(dw));
        ok = (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i, dw) == HAL_OK);
    }
    HAL_FLASH_Lock();
    return ok;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void flashStageEraseAll()
{
    HAL_FLASH_Unlock();
    FLASH->SR = FLASH->SR; // W1C: clear every latched error/status flag

    // Header page first, image pages after — erase one page per HAL call so
    // the watchdog can be fed between the ~22-40ms bus-stalling erases.
    for (uint32_t page = FLASH_STAGING_HEADER_PAGE;
         page < FLASH_STAGING_IMAGE_FIRST_PAGE + FLASH_STAGING_IMAGE_PAGES; page++)
    {
        IWatchdog.reload();
        FLASH_EraseInitTypeDef erase = {};
        erase.TypeErase = FLASH_TYPEERASE_PAGES;
        erase.Banks     = FLASH_BANK_1;
        erase.Page      = page;
        erase.NbPages   = 1;
        uint32_t pageError = 0;
        HAL_FLASHEx_Erase(&erase, &pageError);
    }

    HAL_FLASH_Lock();
    IWatchdog.reload();
}

bool flashStageWriteChunk(uint32_t offset, const uint8_t *data, uint16_t len)
{
    if (len == 0 || len > FLASH_OTA_CHUNK_SIZE ||
        offset % 8 != 0 || offset + len > FLASH_OTA_MAX_IMAGE_SIZE)
    {
        return false;
    }

    // Pad a short trailing chunk with 0xFF up to whole doublewords (0xFF
    // keeps un-owned bytes in the erased state).
    uint8_t buf[FLASH_OTA_CHUNK_SIZE];
    uint32_t padded = (len + 7u) & ~7u;
    memcpy(buf, data, len);
    if (padded > len)
    {
        memset(buf + len, 0xFF, padded - len);
    }

    return programDoublewords(FLASH_STAGING_IMAGE_ADDR + offset, buf, padded);
}

uint32_t flashStageCrc32(uint32_t size)
{
    IWatchdog.reload();
    uint32_t crc = crc32Bytes((const uint8_t *)FLASH_STAGING_IMAGE_ADDR, size);
    IWatchdog.reload();
    return crc;
}

bool flashStageCommitHeader(uint32_t imageSize, uint32_t imageCrc32)
{
    OtaStagingHeader hdr;
    hdr.magic      = FLASH_OTA_HEADER_MAGIC;
    hdr.imageSize  = imageSize;
    hdr.imageCrc32 = imageCrc32;
    hdr.hdrCrc16   = crc16Bytes((const uint8_t *)&hdr, 12);
    hdr.reserved   = 0xFFFF;

    return programDoublewords(FLASH_STAGING_HEADER_ADDR, (const uint8_t *)&hdr, sizeof(hdr));
}
