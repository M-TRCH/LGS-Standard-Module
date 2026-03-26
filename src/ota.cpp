#include "ota.h"
#include "system.h"

// ============================================================
//  CRC-16/MODBUS
// ============================================================
static uint16_t crc16_update(uint16_t crc, uint8_t b)
{
    crc ^= b;
    for (uint8_t i = 0; i < 8; i++)
        crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    return crc;
}

static uint16_t crc16(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++)
        crc = crc16_update(crc, buf[i]);
    return crc;
}

// ============================================================
//  CRC-32 (ISO-HDLC)
// ============================================================
static uint32_t crc32_update(uint32_t crc, uint8_t b)
{
    crc ^= b;
    for (uint8_t i = 0; i < 8; i++)
        crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
    return crc;
}

// ============================================================
//  Flash operations — direct register access (STM32F103)
// ============================================================
static bool flashUnlock()
{
    if (FLASH->CR & FLASH_CR_LOCK)
    {
        FLASH->KEYR = 0x45670123;
        FLASH->KEYR = 0xCDEF89AB;
    }
    return !(FLASH->CR & FLASH_CR_LOCK);
}

static void flashLock()
{
    FLASH->CR |= FLASH_CR_LOCK;
}

static bool flashErasePage(uint32_t pageAddr)
{
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->SR |= FLASH_SR_PGERR | FLASH_SR_WRPRTERR; // Clear errors

    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = pageAddr;
    FLASH->CR |= FLASH_CR_STRT;
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->CR &= ~FLASH_CR_PER;

    return !(FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR));
}

static bool flashWriteData(uint32_t addr, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i += 2)
    {
        uint16_t hw = data[i];
        hw |= (uint16_t)((i + 1 < len) ? data[i + 1] : 0xFF) << 8;

        while (FLASH->SR & FLASH_SR_BSY);
        FLASH->CR |= FLASH_CR_PG;
        *(__IO uint16_t *)(addr + i) = hw;
        while (FLASH->SR & FLASH_SR_BSY);
        FLASH->CR &= ~FLASH_CR_PG;

        if (*(__IO uint16_t *)(addr + i) != hw)
            return false;
    }
    return true;
}

// ============================================================
//  Persistent config — survives OTA firmware updates
//  Stored at OTA_PERSIST_ADDR (last page of app bank)
// ============================================================
static uint16_t calcPersistChecksum(const OtaPersistConfig_t *cfg)
{
    const uint16_t *p = (const uint16_t *)cfg;
    uint16_t cs = 0;
    // XOR all halfwords before the checksum field
    size_t count = offsetof(OtaPersistConfig_t, checksum) / sizeof(uint16_t);
    for (size_t i = 0; i < count; i++)
        cs ^= p[i];
    return cs;
}

bool otaSavePersistentConfig(uint16_t identifier, uint16_t baudRate)
{
    OtaPersistConfig_t cfg;
    cfg.magic      = OTA_PERSIST_MAGIC;
    cfg.identifier = identifier;
    cfg.baudRate   = baudRate;
    cfg.checksum   = calcPersistChecksum(&cfg);

    if (!flashUnlock()) return false;
    if (!flashErasePage(OTA_PERSIST_ADDR)) { flashLock(); return false; }
    bool ok = flashWriteData(OTA_PERSIST_ADDR, (const uint8_t *)&cfg, sizeof(cfg));
    flashLock();
    return ok;
}

bool otaLoadPersistentConfig(uint16_t *identifier, uint16_t *baudRate)
{
    const OtaPersistConfig_t *cfg = (const OtaPersistConfig_t *)OTA_PERSIST_ADDR;
    if (cfg->magic != OTA_PERSIST_MAGIC) return false;
    if (calcPersistChecksum(cfg) != cfg->checksum) return false;
    if (identifier) *identifier = cfg->identifier;
    if (baudRate)   *baudRate   = cfg->baudRate;
    return true;
}

// ============================================================
//  RAM-resident flash copier  (runs from SRAM)
//  Copies staging area -> app area, then system reset.
//  MUST NOT call any flash-resident function.
// ============================================================
__attribute__((section(".RamFunc"), long_call, noinline))
static void ramCopyAndReset(uint32_t src, uint32_t dst, uint32_t size)
{
    __disable_irq();

    // Unlock flash
    FLASH->KEYR = 0x45670123;
    FLASH->KEYR = 0xCDEF89AB;

    // Erase destination pages (skip persistent config page)
    uint32_t pages = (size + OTA_FLASH_PAGE_SIZE - 1) / OTA_FLASH_PAGE_SIZE;
    for (uint32_t p = 0; p < pages; p++)
    {
        uint32_t pageAddr = dst + p * OTA_FLASH_PAGE_SIZE;
        if (pageAddr == OTA_PERSIST_ADDR) continue; // Protect persistent config

        while (FLASH->SR & FLASH_SR_BSY);
        FLASH->SR |= FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
        FLASH->CR |= FLASH_CR_PER;
        FLASH->AR = pageAddr;
        FLASH->CR |= FLASH_CR_STRT;
        while (FLASH->SR & FLASH_SR_BSY);
        FLASH->CR &= ~FLASH_CR_PER;
    }

    // Copy half-words from src to dst
    volatile uint16_t *pSrc = (volatile uint16_t *)src;
    volatile uint16_t *pDst = (volatile uint16_t *)dst;
    uint32_t halfWords = (size + 1) / 2;

    for (uint32_t i = 0; i < halfWords; i++)
    {
        while (FLASH->SR & FLASH_SR_BSY);
        FLASH->CR |= FLASH_CR_PG;
        pDst[i] = pSrc[i];
        while (FLASH->SR & FLASH_SR_BSY);
        FLASH->CR &= ~FLASH_CR_PG;
    }

    FLASH->CR |= FLASH_CR_LOCK;

    // System reset
    __DSB();
    SCB->AIRCR = (0x5FA << SCB_AIRCR_VECTKEY_Pos) | SCB_AIRCR_SYSRESETREQ_Msk;
    __DSB();
    while (1);
}

static HardwareSerial *_otaSer = nullptr;

// ============================================================
//  Protocol — receive frame
//  Returns CMD on success, 0 on timeout/CRC error.
// ============================================================
static uint8_t receiveFrame(uint8_t *buf, uint16_t bufSize,
                            uint16_t *payloadLen, uint32_t timeoutMs)
{
    uint32_t t0 = millis();
    uint8_t  sync = 0;

    // 1) Wait for sync [AA][55]
    while (millis() - t0 < timeoutMs)
    {
        if (!_otaSer->available()) continue;
        uint8_t b = _otaSer->read();
        if      (sync == 0 && b == OTA_SYNC_1) sync = 1;
        else if (sync == 1 && b == OTA_SYNC_2) { sync = 2; break; }
        else    sync = (b == OTA_SYNC_1) ? 1 : 0;
    }
    if (sync != 2) return 0;

    // 2) Read CMD(1) + LEN(2)
    uint8_t hdr[3];
    for (uint8_t i = 0; i < 3; i++)
    {
        while (!_otaSer->available())
            if (millis() - t0 >= timeoutMs) return 0;
        hdr[i] = _otaSer->read();
    }
    uint8_t  cmd = hdr[0];
    uint16_t len = hdr[1] | ((uint16_t)hdr[2] << 8);
    if (len > bufSize) return 0;

    // 3) Read payload
    for (uint16_t i = 0; i < len; i++)
    {
        while (!_otaSer->available())
            if (millis() - t0 >= timeoutMs) return 0;
        buf[i] = _otaSer->read();
    }

    // 4) Read CRC16
    uint8_t crcBuf[2];
    for (uint8_t i = 0; i < 2; i++)
    {
        while (!_otaSer->available())
            if (millis() - t0 >= timeoutMs) return 0;
        crcBuf[i] = _otaSer->read();
    }
    uint16_t rxCrc = crcBuf[0] | ((uint16_t)crcBuf[1] << 8);

    // 5) Verify CRC  (scope: CMD + LEN + PAYLOAD)
    uint16_t calcCrc = 0xFFFF;
    calcCrc = crc16_update(calcCrc, cmd);
    calcCrc = crc16_update(calcCrc, len & 0xFF);
    calcCrc = crc16_update(calcCrc, (len >> 8) & 0xFF);
    for (uint16_t i = 0; i < len; i++)
        calcCrc = crc16_update(calcCrc, buf[i]);

    if (rxCrc != calcCrc) return 0;

    *payloadLen = len;
    return cmd;
}

// ============================================================
//  Public API — otaReceiveFirmware
// ============================================================
bool otaReceiveFirmware(HardwareSerial &serial)
{
    _otaSer = &serial;

    uint8_t  buf[OTA_PACKET_DATA_SIZE + 8];
    uint16_t payloadLen;

    // LED rapid blink to indicate OTA waiting
    Serial.println(F("\n[OTA] ===== OTA Firmware Update Mode ====="));
    Serial.println(F("[OTA] Waiting for firmware on Serial3..."));
    Serial.println(F("[OTA] Send firmware with ota_sender.py"));
    Serial.println(F("[OTA] =====================================\n"));

    // ---- 1. Wait for START ----
    // Blink LED while waiting
    uint32_t waitStart = millis();
    uint8_t  cmd = 0;
    while (millis() - waitStart < OTA_START_TIMEOUT_MS)
    {
        digitalWrite(LED_RUN_PIN, ((millis() / 150) % 2) ? HIGH : LOW);
        if (_otaSer->available())
        {
            cmd = receiveFrame(buf, sizeof(buf), &payloadLen, OTA_START_TIMEOUT_MS - (millis() - waitStart));
            break;
        }
    }

    if (cmd != OTA_CMD_START || payloadLen != 4)
    {
        Serial.println(F("[OTA] Timeout — no START received"));
        digitalWrite(LED_RUN_PIN, LOW);
        return false;
    }

    uint32_t fwSize = (uint32_t)buf[0]
                    | ((uint32_t)buf[1] << 8)
                    | ((uint32_t)buf[2] << 16)
                    | ((uint32_t)buf[3] << 24);

    if (fwSize == 0 || fwSize > OTA_MAX_FW_SIZE)
    {
        Serial.println(F("[OTA] Invalid firmware size"));
        return false;
    }

    Serial.print(F("[OTA] Firmware size: "));
    Serial.print(fwSize);
    Serial.println(F(" bytes"));

    // ---- 2. Erase staging area ----
    Serial.println(F("[OTA] Erasing staging area..."));
    if (!flashUnlock())
    {
        Serial.println(F("[OTA] Flash unlock failed"));
        return false;
    }

    uint32_t pagesToErase = (fwSize + OTA_FLASH_PAGE_SIZE - 1) / OTA_FLASH_PAGE_SIZE;
    for (uint32_t i = 0; i < pagesToErase; i++)
    {
        if (!flashErasePage(OTA_STAGING_ADDR + i * OTA_FLASH_PAGE_SIZE))
        {
            Serial.println(F("[OTA] Flash erase failed"));
            flashLock();
            return false;
        }
    }
    flashLock();

    Serial.print(F("[OTA] Erased "));
    Serial.print(pagesToErase);
    Serial.println(F(" pages"));

    // ---- 3. Receive DATA packets ----
    uint32_t totalRx   = 0;
    uint16_t expectSeq = 0;

    while (totalRx < fwSize)
    {
        digitalWrite(LED_RUN_PIN, ((millis() / 80) % 2) ? HIGH : LOW);

        cmd = receiveFrame(buf, sizeof(buf), &payloadLen, OTA_DATA_TIMEOUT_MS);
        if (cmd != OTA_CMD_DATA)
        {
            Serial.println(F("[OTA] DATA timeout or CRC error"));
            return false;
        }

        if (payloadLen < 3)
        {
            Serial.println(F("[OTA] DATA payload too short"));
            return false;
        }

        uint16_t seq     = buf[0] | ((uint16_t)buf[1] << 8);
        uint16_t dataLen = payloadLen - 2;
        uint8_t *data    = &buf[2];

        if (seq != expectSeq)
        {
            Serial.println(F("[OTA] Sequence mismatch"));
            return false;
        }

        if (!flashUnlock())
        {
            Serial.println(F("[OTA] Flash unlock failed"));
            return false;
        }

        if (!flashWriteData(OTA_STAGING_ADDR + totalRx, data, dataLen))
        {
            Serial.println(F("[OTA] Flash program failed"));
            flashLock();
            return false;
        }
        flashLock();

        totalRx += dataLen;
        expectSeq++;

        if ((expectSeq & 0x1F) == 0) // Every 32 packets (~4KB)
        {
            Serial.print(F("[OTA] "));
            Serial.print(totalRx);
            Serial.print('/');
            Serial.println(fwSize);
        }
    }

    Serial.print(F("[OTA] Received "));
    Serial.print(totalRx);
    Serial.println(F(" bytes total"));

    // ---- 4. Wait for END with CRC32 ----
    cmd = receiveFrame(buf, sizeof(buf), &payloadLen, OTA_DATA_TIMEOUT_MS);
    if (cmd != OTA_CMD_END || payloadLen != 4)
    {
        Serial.println(F("[OTA] END frame missing or malformed"));
        return false;
    }

    uint32_t expectedCrc = (uint32_t)buf[0]
                         | ((uint32_t)buf[1] << 8)
                         | ((uint32_t)buf[2] << 16)
                         | ((uint32_t)buf[3] << 24);

    // ---- 5. Verify CRC32 of staged firmware ----
    Serial.println(F("[OTA] Verifying CRC32..."));
    uint32_t calcCrc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < fwSize; i++)
        calcCrc = crc32_update(calcCrc, *(__IO uint8_t *)(OTA_STAGING_ADDR + i));
    calcCrc = ~calcCrc;

    if (calcCrc != expectedCrc)
    {
        Serial.print(F("[OTA] CRC mismatch! expected=0x"));
        Serial.print(expectedCrc, HEX);
        Serial.print(F(" got=0x"));
        Serial.println(calcCrc, HEX);
        return false;
    }

    Serial.println(F("[OTA] CRC32 OK"));

    // ---- 6. Apply: copy staging -> app, then reset ----
    Serial.println(F("[OTA] Applying firmware update..."));
    Serial.println(F("[OTA] >>> Copying to 0x08000000 and resetting <<<"));
    Serial.flush();
    delay(50);

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    ramCopyAndReset(OTA_STAGING_ADDR, OTA_APP_ADDR, fwSize);

    // Should never reach here
    while (1);
    return true;
}
