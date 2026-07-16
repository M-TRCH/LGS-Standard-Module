/*  LGS R5.0 bootloader (env LGS_BOOT, framework = cmsis, 4KB slot).
 *
 *  Deliberately dumb: no UART, no I2C, no clocks beyond the reset HSI16.
 *  All download logic lives in the application; this stage only applies a
 *  fully-verified staged image and is idempotent against power loss:
 *
 *    1. Reload the IWDG unconditionally — the app's watchdog SURVIVES
 *       NVIC_SystemReset, so we may arrive with it armed and counting.
 *    2. Read the staging header (magic + size + CRC32 + header CRC16).
 *       No/invalid header -> just boot the app.
 *    3. Header valid: CRC32 the staged image. Mismatch -> erase the header
 *       (drop the bad request) and boot the old app untouched.
 *    4. Match: erase the app slot, copy doubleword-wise, CRC32-verify the
 *       app slot, and only THEN erase the staging header. Power loss at any
 *       point before that final erase leaves the header valid, so the next
 *       boot simply copies again. Then reset.
 *    5. Jump to the app after a sanity check of its vector table.
 *
 *  Shared layout contract: include/flash_layout.h (also used by the app).
 */

#include "stm32g0xx.h"
#include "flash_layout.h"

#define RAM_START 0x20000000u
#define RAM_END   0x20009000u   /* 36KB */

static inline void iwdgReload(void)
{
    IWDG->KR = 0x0000AAAAu; /* harmless when the IWDG was never started */
}

/* --- CRCs (bitwise, table-free — size over speed) --------------------- */

static uint32_t crc32Image(const uint8_t *p, uint32_t len)
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

static uint16_t crc16Header(const uint8_t *p, uint32_t len)
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

/* --- Flash driver (direct registers, G0: 2KB pages, 8-byte program) --- */

static void flashWait(void)
{
    while (FLASH->SR & FLASH_SR_BSY1) {}
}

static void flashUnlock(void)
{
    if (FLASH->CR & FLASH_CR_LOCK)
    {
        FLASH->KEYR = 0x45670123u;
        FLASH->KEYR = 0xCDEF89ABu;
    }
}

static void flashClearErrors(void)
{
    FLASH->SR = FLASH->SR; /* W1C every latched flag */
}

static void flashErasePage(uint32_t page)
{
    iwdgReload();
    flashWait();
    flashClearErrors();
    FLASH->CR = (FLASH->CR & ~(FLASH_CR_PNB_Msk | FLASH_CR_PER)) |
                (page << FLASH_CR_PNB_Pos) | FLASH_CR_PER;
    FLASH->CR |= FLASH_CR_STRT;
    flashWait();
    FLASH->CR &= ~FLASH_CR_PER;
}

static void flashProgramDoubleword(uint32_t addr, uint64_t data)
{
    flashWait();
    flashClearErrors();
    FLASH->CR |= FLASH_CR_PG;
    *(volatile uint32_t *)addr = (uint32_t)data;
    __ISB();
    *(volatile uint32_t *)(addr + 4u) = (uint32_t)(data >> 32);
    flashWait();
    FLASH->CR &= ~FLASH_CR_PG;
}

/* --- Apply / jump ------------------------------------------------------ */

static void applyStagedImage(uint32_t size, uint32_t crc32)
{
    flashUnlock();

    for (uint32_t p = FLASH_APP_FIRST_PAGE; p < FLASH_APP_FIRST_PAGE + FLASH_APP_PAGES; p++)
    {
        flashErasePage(p);
    }

    /* Copy padded to whole doublewords; staging beyond the image is erased
     * (0xFF), so reading a full trailing doubleword is safe. */
    uint32_t doublewords = (size + 7u) / 8u;
    const uint64_t *src = (const uint64_t *)FLASH_STAGING_IMAGE_ADDR;
    for (uint32_t i = 0; i < doublewords; i++)
    {
        if ((i & 63u) == 0u)
        {
            iwdgReload();
        }
        flashProgramDoubleword(FLASH_APP_ADDR + i * 8u, src[i]);
    }

    iwdgReload();
    if (crc32Image((const uint8_t *)FLASH_APP_ADDR, size) == crc32)
    {
        /* Copy verified: consume the request. This is the commit point. */
        flashErasePage(FLASH_STAGING_HEADER_PAGE);
    }
    /* Verify failed: leave the header, the next boot retries the copy. */
    NVIC_SystemReset();
}

static void jumpToApp(void)
{
    uint32_t sp = *(volatile uint32_t *)FLASH_APP_ADDR;
    uint32_t pc = *(volatile uint32_t *)(FLASH_APP_ADDR + 4u);

    if (sp < RAM_START || sp > RAM_END ||
        pc < FLASH_APP_ADDR || pc >= FLASH_STAGING_HEADER_ADDR)
    {
        /* No plausible app (blank slot): idle here, keeping any armed
         * watchdog fed, until someone rescues the board over ST-Link. */
        for (;;)
        {
            iwdgReload();
        }
    }

    __set_MSP(sp);
    ((void (*)(void))pc)();
    for (;;) {}
}

int main(void)
{
    iwdgReload();

    const OtaStagingHeader *hdr = (const OtaStagingHeader *)FLASH_STAGING_HEADER_ADDR;
    if (hdr->magic == FLASH_OTA_HEADER_MAGIC &&
        hdr->imageSize >= 8u && hdr->imageSize <= FLASH_OTA_MAX_IMAGE_SIZE &&
        crc16Header((const uint8_t *)hdr, 12u) == hdr->hdrCrc16)
    {
        iwdgReload();
        if (crc32Image((const uint8_t *)FLASH_STAGING_IMAGE_ADDR, hdr->imageSize) == hdr->imageCrc32)
        {
            applyStagedImage(hdr->imageSize, hdr->imageCrc32); /* resets */
        }

        /* Staged bytes don't match their own header: drop the request so
         * every future boot isn't burned re-checking a corpse. */
        flashUnlock();
        flashErasePage(FLASH_STAGING_HEADER_PAGE);
    }

    jumpToApp();
    return 0;
}
