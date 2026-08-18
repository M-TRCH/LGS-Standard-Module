#include "svc/stats.h"
#include "config.h"
#include "drivers/eeprom_at24.h"
#include "svc/modbus_map.h"
#include "svc/modbus_server.h"
#include <string.h>

namespace {

// --- Blob layouts (AT24 @ STATS_AT24_ADDR, 128-byte reservation) -----------
//
// v1 (fw <= v3.2.0) had no version field: magic + CRC only, with a reserved
// u16 at offset 6 that was always written 0. v2 turns that slot into the
// version. The v1 crc position (offset 72) is DATA in v2 (latchFires), so
// validation must try v2 first and only then fall back to the v1 prefix.
// Downgrade is safe by construction: v3.2.0 reading a v2 blob fails its CRC,
// zeroes its counters and rewrites a valid v1 blob; a later re-upgrade
// imports that v1 blob again.

struct StatsBlobV1
{
    uint32_t magic;
    uint16_t bootCount;
    uint16_t reserved;                          // always 0 in v1
    uint32_t onCount[MB_LED_PRESET_COUNT];
    uint32_t onTimeS[MB_LED_PRESET_COUNT];
    uint16_t crc;                               // CRC16-CCITT over magic..onTimeS
};

struct StatsBlobV2
{
    uint32_t magic;
    uint16_t bootCount;
    uint16_t version;                           // = 2 (v1 wrote 0 here)
    uint32_t onCount[MB_LED_PRESET_COUNT];      // same offsets as v1
    uint32_t onTimeS[MB_LED_PRESET_COUNT];
    uint32_t latchFires;                        // occupies v1's crc position
    uint32_t buttonPresses;
    uint32_t opSeconds;
    uint16_t iwdgResets;                        // saturates at 0xFFFF
    uint16_t seq;                               // A/B slot age; v3.3.0 wrote 0
    uint16_t crc;                               // CRC16-CCITT over magic..seq
};

constexpr uint32_t STATS_MAGIC = 0x4C475353;    // 'LGSS'
constexpr uint16_t STATS_VERSION = 2;

static_assert(sizeof(StatsBlobV1) == 76, "v1 wire layout");
static_assert(offsetof(StatsBlobV1, crc) == 72, "v1 crc position");
static_assert(sizeof(StatsBlobV2) == 92, "v2 wire layout");
static_assert(offsetof(StatsBlobV2, crc) == 88, "v2 crc position");
static_assert(offsetof(StatsBlobV2, seq) == 86, "seq reuses v3.3.0's reserved2");
static_assert(offsetof(StatsBlobV2, onCount) == 8, "counter arrays must not move");
static_assert(offsetof(StatsBlobV2, latchFires) == 72, "v2-first validation depends on this");
// <= 94 bytes keeps the write at 5 AT24 page cycles (30/2/30/2/30 chunking
// from a 32-aligned base); 95 would add a sixth.
static_assert(sizeof(StatsBlobV2) <= 94, "EEPROM write-cycle cliff");
static_assert(STATS_AT24_ADDR + sizeof(StatsBlobV2) <= COMMISSION_AT24_ADDR,
              "stats blob must not overlap the commissioning record");

// --- State -----------------------------------------------------------------

uint16_t bootCount = 0;
uint32_t onCount[MB_LED_PRESET_COUNT] = {};
uint32_t onTimeS[MB_LED_PRESET_COUNT] = {};
uint32_t latchFires = 0;
uint32_t buttonPresses = 0;
uint32_t opSeconds = 0;                         // persisted base, folded lazily
uint16_t iwdgResets = 0;
bool pendingIwdg = false;                       // noted at boot, folded at commit

uint32_t opStampMs = 0;                         // millis() of the last fold
uint16_t opFracMs = 0;                          // sub-second remainder (<1000)

StatsBlobV2 persistedStats = {};                // change-detection cache
bool     liveSlotB = false;                     // which slot holds the newest
uint16_t slotSeq = 0;                           // its sequence number

uint16_t statsCrc16(const uint8_t *p, size_t len)
{
    uint16_t crc = 0xFFFF;
    while (len--)
    {
        crc ^= (uint16_t)(*p++) << 8;
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

void statsFill(StatsBlobV2 &b)
{
    memset(&b, 0, sizeof(b));   // tail padding must be 0 for exact memcmp
    b.magic = STATS_MAGIC;
    b.bootCount = bootCount;
    b.version = STATS_VERSION;
    memcpy(b.onCount, onCount, sizeof(onCount));
    memcpy(b.onTimeS, onTimeS, sizeof(onTimeS));
    b.latchFires = latchFires;
    b.buttonPresses = buttonPresses;
    b.opSeconds = opSeconds;
    b.iwdgResets = iwdgResets;
    b.seq = slotSeq;            // the live sequence: only a real write bumps it
    b.crc = statsCrc16((const uint8_t *)&b, offsetof(StatsBlobV2, crc));
}

// Fold the running operating-time into opSeconds (mutating). Only the
// persist path calls this; publication computes a live value on the side.
void foldOpSeconds()
{
    uint32_t now = millis();
    uint32_t deltaMs = (now - opStampMs) + opFracMs;
    opSeconds += deltaMs / 1000;
    opFracMs = (uint16_t)(deltaMs % 1000);
    opStampMs = now;
}

uint16_t clampToU16(uint32_t value)
{
    return (value > 65535) ? 65535 : (uint16_t)value;
}

// u32 into a hi/lo register pair, hi word first (uptime/UID convention).
void pub32(uint16_t addr, uint32_t value)
{
    mbRegWrite(addr, (uint16_t)(value >> 16));
    mbRegWrite((uint16_t)(addr + 1), (uint16_t)value);
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// True when `a` is the younger of two slot sequences, wrap included.
static bool seqNewer(uint16_t a, uint16_t b)
{
    return (int16_t)(a - b) > 0;
}

static bool readSlot(uint16_t addr, StatsBlobV2 &out)
{
    return at24Read(addr, (uint8_t *)&out, sizeof(out)) &&
           out.magic == STATS_MAGIC &&
           out.version == STATS_VERSION &&
           out.crc == statsCrc16((const uint8_t *)&out, offsetof(StatsBlobV2, crc));
}

void statsInit()
{
    // Both slots, newest valid one wins. A torn write leaves its own slot
    // invalid (or older); the other one is still whole, which is the whole
    // point of writing them alternately.
    StatsBlobV2 a, b, v2;
    const bool okA = readSlot(STATS_AT24_ADDR, a);
    const bool okB = readSlot(STATS_AT24_ADDR_B, b);
    bool have = false;
    if (okA && okB)
    {
        v2 = seqNewer(b.seq, a.seq) ? b : a;
        liveSlotB = seqNewer(b.seq, a.seq);
        have = true;
    }
    else if (okA) { v2 = a; liveSlotB = false; have = true; }
    else if (okB) { v2 = b; liveSlotB = true;  have = true; }

    if (have)
    {
        bootCount = v2.bootCount;
        memcpy(onCount, v2.onCount, sizeof(onCount));
        memcpy(onTimeS, v2.onTimeS, sizeof(onTimeS));
        latchFires = v2.latchFires;
        buttonPresses = v2.buttonPresses;
        opSeconds = v2.opSeconds;
        iwdgResets = v2.iwdgResets;
        slotSeq = v2.seq;
        persistedStats = v2;
    }
    else
    {
        StatsBlobV1 v1;
        if (at24Read(STATS_AT24_ADDR, (uint8_t *)&v1, sizeof(v1)) &&
            v1.magic == STATS_MAGIC &&
            v1.crc == statsCrc16((const uint8_t *)&v1, offsetof(StatsBlobV1, crc)))
        {
            // One-time import from a v3.2.0-or-older blob: the counters it
            // has survive, the new ones start at zero. The persist cache
            // stays zeroed so the first commit is guaranteed to write the
            // upgraded blob.
            bootCount = v1.bootCount;
            memcpy(onCount, v1.onCount, sizeof(onCount));
            memcpy(onTimeS, v1.onTimeS, sizeof(onTimeS));
        }
        // else: blank or torn — every counter starts at zero.
    }
    opStampMs = millis();
}

uint16_t statsBootCommit()
{
    bootCount++;
    if (pendingIwdg)
    {
        if (iwdgResets != 0xFFFF)
        {
            iwdgResets++;
        }
        pendingIwdg = false;
    }
    statsPersistIfChanged();    // the one EEPROM write of a normal boot
    return bootCount;
}

void statsNoteResetCause(uint16_t causeBits)
{
    if (causeBits & (1u << 0))  // bit0 = IWDG, reg-8 encoding
    {
        pendingIwdg = true;
    }
}

void statsNoteLedOn(uint8_t preset)
{
    if (preset >= 1 && preset <= MB_LED_PRESET_COUNT)
    {
        onCount[preset - 1]++;
    }
}

void statsAddOnTime(uint8_t preset, uint32_t seconds)
{
    if (preset >= 1 && preset <= MB_LED_PRESET_COUNT)
    {
        onTimeS[preset - 1] += seconds;
    }
}

void statsNoteLatchFire()
{
    latchFires++;
}

void statsNotePress()
{
    buttonPresses++;
}

void statsPersistIfChanged()
{
    foldOpSeconds();
    StatsBlobV2 b;
    statsFill(b);
    if (memcmp(&b, &persistedStats, sizeof(b)) == 0)
    {
        return; // unchanged: spare the EEPROM the write cycle
    }
    // Write the slot that is NOT live, then flip: until this write completes
    // and validates, the previous copy is the one statsInit() would pick.
    b.seq = (uint16_t)(slotSeq + 1);
    b.crc = statsCrc16((const uint8_t *)&b, offsetof(StatsBlobV2, crc));
    const uint16_t addr = liveSlotB ? STATS_AT24_ADDR : STATS_AT24_ADDR_B;
    if (at24Write(addr, (const uint8_t *)&b, sizeof(b)))
    {
        liveSlotB = !liveSlotB;
        slotSeq = b.seq;
        persistedStats = b;
    }
}

void statsClearUsage()
{
    memset(onCount, 0, sizeof(onCount));
    memset(onTimeS, 0, sizeof(onTimeS));
    latchFires = 0;
    buttonPresses = 0;
    opSeconds = 0;
    iwdgResets = 0;
    // Re-stamp so operating-time accrued before the clear cannot leak back
    // in at the next fold.
    opStampMs = millis();
    opFracMs = 0;
    statsPersistIfChanged();
}

void statsPublishRegisters(uint32_t now)
{
    (void)now;

    // Legacy clamped view (200-281): totals + per-preset, exactly the wire
    // contract v3.2.0 masters read.
    uint32_t totalCount = 0;
    uint32_t totalTimeS = 0;
    for (uint16_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
    {
        mbRegWrite(mbRegLedOnCounter(n), clampToU16(onCount[n - 1]));
        mbRegWrite(mbRegLedOnTime(n), clampToU16(onTimeS[n - 1]));
        totalCount += onCount[n - 1];
        totalTimeS += onTimeS[n - 1];
    }
    mbRegWrite(MB_REG_TOTAL_LED_ON_CNT, clampToU16(totalCount));
    mbRegWrite(MB_REG_TOTAL_LED_ON_TIME, clampToU16(totalTimeS));

    // Statistics v2 (400-451): true u32 values, hi word first. Operating
    // seconds are published live (base + un-folded remainder) without
    // mutating the fold state. 411-419 stay 0 (reserved, server memsets).
    pub32(MB_REG_S2_TOTAL_ON_CNT_HI, totalCount);
    pub32(MB_REG_S2_TOTAL_ON_TIME_HI, totalTimeS);
    pub32(MB_REG_S2_LATCH_FIRES_HI, latchFires);
    pub32(MB_REG_S2_BTN_PRESSES_HI, buttonPresses);
    pub32(MB_REG_S2_OP_SECONDS_HI,
          opSeconds + ((millis() - opStampMs) + opFracMs) / 1000);
    mbRegWrite(MB_REG_S2_IWDG_RESETS, iwdgResets);
    for (uint16_t n = 1; n <= MB_LED_PRESET_COUNT; n++)
    {
        pub32(mbRegS2OnCounterHi(n), onCount[n - 1]);
        pub32(mbRegS2OnTimeHi(n), onTimeS[n - 1]);
    }
}
