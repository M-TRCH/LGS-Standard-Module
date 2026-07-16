#include "app/ota_control.h"
#include "config.h"
#include "flash_layout.h"
#include "app/display_control.h"
#include "app/latch_control.h"
#include "app/ops.h"
#include "drivers/flash_stage.h"
#include "svc/modbus_map.h"
#include "svc/modbus_server.h"

// ---------------------------------------------------------------------------
// Session state
// ---------------------------------------------------------------------------

namespace {

enum OtaState : uint8_t
{
    OTA_IDLE      = 0,
    OTA_RECEIVING = 1,
    OTA_VERIFIED  = 2,
    OTA_FAILED    = 3,
};

enum OtaError : uint8_t
{
    OTA_ERR_NONE        = 0,
    OTA_ERR_BAD_SIZE    = 1,
    OTA_ERR_BAD_CHUNKS  = 2,
    OTA_ERR_CRC32       = 3,
    OTA_ERR_TIMEOUT     = 4,
    OTA_ERR_FLASH       = 5,
    OTA_ERR_NOT_VERIFIED= 6,
    OTA_ERR_LATCH_BUSY  = 7,
    OTA_ERR_INCOMPLETE  = 8,
};

OtaState state = OTA_IDLE;
uint32_t imageSize = 0;
uint32_t imageCrc32 = 0;
uint16_t totalChunks = 0;
uint16_t chunksReceived = 0;
uint32_t lastActivityMs = 0;
uint8_t lastShownPercent = 0xFF;

void publishState(OtaState s, OtaError err = OTA_ERR_NONE)
{
    state = s;
    mbRegWrite(MB_REG_OTA_STATE, (uint16_t)((err << 8) | s));
}

// The received bitmap LIVES in the Modbus holding registers (360-389) —
// masters read it back with FC03 to find lost chunks.
bool bitmapTest(uint16_t chunk)
{
    uint16_t reg = mbRegRead(MB_REG_OTA_BITMAP_FIRST + chunk / 16);
    return (reg >> (chunk % 16)) & 1u;
}

void bitmapSet(uint16_t chunk)
{
    uint16_t addr = MB_REG_OTA_BITMAP_FIRST + chunk / 16;
    mbRegWrite(addr, mbRegRead(addr) | (uint16_t)(1u << (chunk % 16)));
}

void bitmapClearAll()
{
    for (uint16_t addr = MB_REG_OTA_BITMAP_FIRST; addr <= MB_REG_OTA_BITMAP_LAST; addr++)
    {
        mbRegWrite(addr, 0);
    }
}

// CRC16-CCITT over the chunk payload (matches the host tool).
uint16_t crc16Payload(const uint8_t *p, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    while (len--)
    {
        crc ^= (uint16_t)(*p++) << 8;
        for (uint8_t i = 0; i < 8; i++)
        {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

void resetSession()
{
    imageSize = 0;
    imageCrc32 = 0;
    totalChunks = 0;
    chunksReceived = 0;
    lastShownPercent = 0xFF;
    mbRegWrite(MB_REG_OTA_CHUNKS_RX, 0);
    bitmapClearAll();
}

void showProgress()
{
    // OTA progress reuses the display feature: percent 0-99 as the big
    // number. Rendered only when the percent changes (~100 renders/session)
    // — a 20ms OLED transfer between chunks; the 256B RX ring buffers the
    // next broadcast frame meanwhile.
    uint8_t percent = (uint8_t)(((uint32_t)chunksReceived * 100u) / totalChunks);
    if (percent > 99)
    {
        percent = 99;
    }
    if (percent != lastShownPercent)
    {
        lastShownPercent = percent;
        displayControlShowNumber(percent);
        displayControlSetEnabled(true);
    }
}

// --- Modbus handlers ---

// Enter OTA (coil 505, broadcast): metadata must already sit in 284-288.
void onOtaEnter(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;
    mbCoilWrite(MB_COIL_OTA_ENTER, false);

    uint32_t size = ((uint32_t)mbRegRead(MB_REG_OTA_SIZE_HI) << 16) | mbRegRead(MB_REG_OTA_SIZE_LO);
    uint32_t crc  = ((uint32_t)mbRegRead(MB_REG_OTA_CRC_HI) << 16) | mbRegRead(MB_REG_OTA_CRC_LO);
    uint16_t chunks = mbRegRead(MB_REG_OTA_TOTAL_CHUNKS);

    if (!latchControlFsmIdle())
    {
        publishState(OTA_FAILED, OTA_ERR_LATCH_BUSY);
        return;
    }
    if (size < 8 || size > FLASH_OTA_MAX_IMAGE_SIZE)
    {
        publishState(OTA_FAILED, OTA_ERR_BAD_SIZE);
        return;
    }
    if (chunks == 0 || chunks != (size + FLASH_OTA_CHUNK_SIZE - 1) / FLASH_OTA_CHUNK_SIZE)
    {
        publishState(OTA_FAILED, OTA_ERR_BAD_CHUNKS);
        return;
    }

    resetSession();
    imageSize = size;
    imageCrc32 = crc;
    totalChunks = chunks;

    flashStageEraseAll();       // ~700ms of page erases (IWDG fed inside)

    lastActivityMs = millis();
    publishState(OTA_RECEIVING);
    showProgress();             // 0% on the OLED
}

// Chunk commit (reg 357, REG_CHANGE): the master bumps this on EVERY
// transmission (including retransmits), so it always fires. A chunk that
// fails its CRC16 is silently ignored — the bitmap repair rounds re-send it.
void onOtaCommit(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;

    if (state != OTA_RECEIVING)
    {
        return;
    }
    lastActivityMs = millis();

    uint16_t idx = mbRegRead(MB_REG_OTA_CHUNK_INDEX);
    uint16_t len = mbRegRead(MB_REG_OTA_CHUNK_LEN);
    uint16_t expectedLen = (idx == totalChunks - 1)
        ? (uint16_t)(imageSize - (uint32_t)idx * FLASH_OTA_CHUNK_SIZE)
        : (uint16_t)FLASH_OTA_CHUNK_SIZE;

    if (idx >= totalChunks || len != expectedLen)
    {
        return; // malformed frame: drop, repair rounds handle it
    }
    if (bitmapTest(idx))
    {
        return; // duplicate (retransmit of a chunk we already own): skip
    }

    // Unpack the payload window (big-endian bytes, two per register).
    uint8_t payload[FLASH_OTA_CHUNK_SIZE];
    for (uint16_t i = 0; i < len; i++)
    {
        uint16_t reg = mbRegRead(MB_REG_OTA_DATA_FIRST + i / 2);
        payload[i] = (i & 1) ? (uint8_t)reg : (uint8_t)(reg >> 8);
    }
    if (crc16Payload(payload, len) != mbRegRead(MB_REG_OTA_CHUNK_CRC))
    {
        return; // corrupt on the wire: drop
    }

    if (!flashStageWriteChunk((uint32_t)idx * FLASH_OTA_CHUNK_SIZE, payload, len))
    {
        publishState(OTA_FAILED, OTA_ERR_FLASH);
        return;
    }

    bitmapSet(idx);
    chunksReceived++;
    mbRegWrite(MB_REG_OTA_CHUNKS_RX, chunksReceived);
    showProgress();
}

// Finalize (coil 506): all chunks in -> CRC32 the staged image.
void onOtaFinalize(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;
    mbCoilWrite(MB_COIL_OTA_FINALIZE, false);

    if (state != OTA_RECEIVING)
    {
        return;
    }
    lastActivityMs = millis();

    if (chunksReceived != totalChunks)
    {
        publishState(OTA_RECEIVING, OTA_ERR_INCOMPLETE); // stays receiving: repair continues
        return;
    }
    if (flashStageCrc32(imageSize) == imageCrc32)
    {
        publishState(OTA_VERIFIED);
    }
    else
    {
        publishState(OTA_FAILED, OTA_ERR_CRC32);
    }
}

// Apply (coil 507): verified only — commit the header and reboot into the
// bootloader, which copies the staged image into the app slot.
void onOtaApply(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;
    mbCoilWrite(MB_COIL_OTA_APPLY, false);

    if (state != OTA_VERIFIED)
    {
        publishState(state, OTA_ERR_NOT_VERIFIED);
        return;
    }
    if (!flashStageCommitHeader(imageSize, imageCrc32))
    {
        publishState(OTA_FAILED, OTA_ERR_FLASH);
        return;
    }
    opsSystemReset(); // MOSFET low first, then NVIC_SystemReset -> bootloader
}

// Abort (coil 508): drop the session, back to idle.
void onOtaAbort(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;
    mbCoilWrite(MB_COIL_OTA_ABORT, false);

    resetSession();
    publishState(OTA_IDLE);
    displayControlSetEnabled(false);
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void otaControlInit()
{
    mbRegisterHandler(MB_WATCH_COIL_COMMAND, MB_COIL_OTA_ENTER, onOtaEnter);
    mbRegisterHandler(MB_WATCH_COIL_COMMAND, MB_COIL_OTA_FINALIZE, onOtaFinalize);
    mbRegisterHandler(MB_WATCH_COIL_COMMAND, MB_COIL_OTA_APPLY, onOtaApply);
    mbRegisterHandler(MB_WATCH_COIL_COMMAND, MB_COIL_OTA_ABORT, onOtaAbort);
    mbRegisterHandler(MB_WATCH_REG_CHANGE, MB_REG_OTA_COMMIT, onOtaCommit);
    publishState(OTA_IDLE);
}

void otaControlTick(uint32_t now)
{
    if (state == OTA_RECEIVING && now - lastActivityMs > OTA_SESSION_TIMEOUT_MS)
    {
        resetSession();
        publishState(OTA_FAILED, OTA_ERR_TIMEOUT);
        displayControlSetEnabled(false);
    }
}
