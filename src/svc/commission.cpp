#include "svc/commission.h"

#include <Arduino.h>

#include "config.h"
#include "version.h"
#include "drivers/eeprom_at24.h"
#include "svc/settings.h"

// ---------------------------------------------------------------------------
// The block itself
// ---------------------------------------------------------------------------

static_assert(sizeof(CommissionBlock) == COMMISSION_BLOCK_SIZE,
              "CommissionBlock must stay 32 bytes: the host patches it by offset");

/*  Defined here, read at runtime by commissionRead(), and never written.
 *
 *  Three things keep it in the binary where a host tool can find it:
 *  external linkage stops LTO from internalising it, `used` stops it being
 *  dropped as unreferenced, and `volatile` stops the values being folded into
 *  the code that reads them (which would leave nothing to patch). None of
 *  that is a guarantee across toolchain versions — tools/post_build_check.py
 *  is, by failing the build if the block is not in firmware.bin exactly once.
 *
 *  crc is filled in by the host when it patches; as built it covers the
 *  shipped values, so post_build_check can verify the block it finds.
 */
extern const volatile CommissionBlock gCommissionBlock;

__attribute__((used))
const volatile CommissionBlock gCommissionBlock =
{
    .magic      = COMMISSION_MAGIC,
    .version    = COMMISSION_VERSION,
    .size       = sizeof(CommissionBlock),
    .tokenLo    = 0,                    // un-patched: nothing to apply
    .tokenHi    = 0,
    .applyMask  = 0,
    .flags      = 0,
    .identifier = DEFAULT_IDENTIFIER,
    .deviceType = DEVICE_TYPE,
    .reserved   = 0,
    .crc        = 0xC8B0,               // over the values above; post_build_check.py
                                        // recomputes it and prints the right
                                        // constant if a default ever changes
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

constexpr uint32_t RECORD_MAGIC   = 0x4C475350;  // 'LGSP'
constexpr uint16_t RECORD_VERSION = 1;

/*  Which patched image this board has already consumed. 16 bytes at a
 *  32-aligned address, so at24Write lays it down in a single page write — a
 *  power loss can only leave the whole record bad-CRC, never half of a new
 *  token over half of an old one.
 */
struct ProvisioningRecord
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t token;
    // Was `reserved`, always written 0 by the memset in writeToken(), so
    // every board commissioned before this field existed already reads 0 —
    // which is not a valid device type and therefore means "not recorded,
    // use the compile-time default". No record version bump, no migration.
    uint16_t deviceType;
    uint16_t crc;           // CRC16-CCITT over the bytes above
};

static_assert(sizeof(ProvisioningRecord) == 16,
              "record must stay 16 bytes to fit one AT24 page write");
static_assert(STATS_AT24_ADDR + 128 <= COMMISSION_AT24_ADDR,
              "the commissioning record must not overlap the statistics blob");

// CRC16-CCITT, same parameters as the settings blob (poly 0x1021, init
// 0xFFFF) so there is one checksum to reason about across the firmware.
uint16_t crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

/*  @brief The token this board already consumed, or 0 if there is none.
 *
 *  A missing, unreadable or damaged record reads as "nothing consumed", so
 *  commissioning retries rather than skips. Retrying is the safe direction:
 *  the worst it can do is set an ID that is already set.
 */
bool readRecord(ProvisioningRecord &r)
{
    if (!at24Read(COMMISSION_AT24_ADDR, (uint8_t *)&r, sizeof(r)))
    {
        return false;
    }
    if (r.magic != RECORD_MAGIC || r.version != RECORD_VERSION || r.size != sizeof(r))
    {
        return false;
    }
    return r.crc == crc16((const uint8_t *)&r, offsetof(ProvisioningRecord, crc));
}

uint32_t storedToken()
{
    ProvisioningRecord r;
    return readRecord(r) ? r.token : 0;
}

bool writeRecord(uint32_t token, uint16_t deviceType)
{
    ProvisioningRecord r;
    memset(&r, 0, sizeof(r));
    r.magic      = RECORD_MAGIC;
    r.version    = RECORD_VERSION;
    r.size       = sizeof(r);
    r.token      = token;
    r.deviceType = deviceType;
    r.crc        = crc16((const uint8_t *)&r, offsetof(ProvisioningRecord, crc));
    return at24Write(COMMISSION_AT24_ADDR, (const uint8_t *)&r, sizeof(r));
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool commissionRead(CommissionBlock *out)
{
    // Copy byte-wise through a volatile pointer, deliberately NOT with memcpy:
    // memcpy's parameters are not volatile, so handing it the block would cast
    // the qualifier away and hand the optimiser back the freedom to fold these
    // values into the code — leaving the host tool patching bytes nothing
    // actually reads. This loop is the whole reason the block is patchable.
    CommissionBlock b;
    const volatile uint8_t *src = (const volatile uint8_t *)&gCommissionBlock;
    uint8_t *dst = (uint8_t *)&b;
    for (size_t i = 0; i < sizeof(b); i++)
    {
        dst[i] = src[i];
    }

    if (memcmp(b.magic, COMMISSION_MAGIC, sizeof(COMMISSION_MAGIC)) != 0)
    {
        return false;
    }
    // v1 images (32 bytes, ID only) stay valid: their crc sits where v2 keeps
    // deviceType, so the shorter block is re-read into the right fields and
    // deviceType left 0 = "not specified". A tool that only knows v1 must
    // keep working — some of those images are already in the field.
    if (b.version == 1 && b.size == COMMISSION_BLOCK_SIZE_V1)
    {
        CommissionBlock v1;
        memset(&v1, 0, sizeof(v1));
        memcpy(&v1, &b, COMMISSION_BLOCK_SIZE_V1);
        const uint16_t crc = *(const uint16_t *)((const uint8_t *)&b
                                                 + COMMISSION_BLOCK_SIZE_V1 - 2);
        if (crc16((const uint8_t *)&b, COMMISSION_BLOCK_SIZE_V1 - 2) != crc)
        {
            return false;
        }
        v1.deviceType = 0;
        v1.crc = crc;
        *out = v1;
        return true;
    }
    if (b.version != COMMISSION_VERSION || b.size != sizeof(CommissionBlock))
    {
        return false;
    }
    if (crc16((const uint8_t *)&b, COMMISSION_CRC_LEN) != b.crc)
    {
        return false;
    }

    *out = b;
    return true;
}

// What the boot-time I2C2 probe found: -1 not probed yet, 0 nothing answered,
// 1 an OLED answered.
static int8_t displayFitted = -1;

// The type to assume for a board that has a display but no recorded type.
// It can only be a ring type, because a board with an OLED is by definition
// not a STANDARD one, so a build defaulted to STANDARD is corrected here.
static uint16_t ringTypeDefault()
{
    return ((uint16_t)DEVICE_TYPE == DEVICE_TYPE_STANDARD)
               ? (uint16_t)DEVICE_TYPE_NARCOTIC
               : (uint16_t)DEVICE_TYPE;
}

uint16_t deviceTypeEffective()
{
    const uint16_t recorded = commissionDeviceType();

    // An ACK is proof. Something answered at the OLED's address, so this
    // board has a display, so it is a ring board — and a record that says
    // STANDARD is simply wrong. Believing it would drive the 8-LED mask and
    // leave the ring dark, which on the shelf cannot be told from a dead
    // module.
    if (displayFitted > 0)
    {
        return (recorded && recorded != DEVICE_TYPE_STANDARD) ? recorded
                                                              : ringTypeDefault();
    }

    // Silence is NOT proof. A STANDARD board has no OLED — but neither does a
    // ring board whose OLED has died, and treating the second as STANDARD
    // would put out its ring as well, losing the whole module instead of just
    // its display. So a recorded type always wins here, and the probe only
    // answers the question nothing else can.
    if (recorded)
    {
        return recorded;
    }
    if (displayFitted == 0)
    {
        return DEVICE_TYPE_STANDARD;
    }
    return (uint16_t)DEVICE_TYPE;   // never probed, never recorded
}

void deviceTypeResolveFromDisplay(bool oledFitted)
{
    displayFitted = oledFitted ? 1 : 0;

    // Then remember it, once, in the field the commissioning record already
    // carries — so silence never has to be interpreted twice. From the next
    // boot on there IS a recorded answer, which is what lets a ring board
    // keep its ring on the day its OLED dies. One page write in a board's
    // lifetime; skipped when it cannot be persisted, and when the answer is
    // already there (including the one commissionApplyAtBoot just wrote).
    if (!settingsStorageOk() || commissionDeviceType() != 0)
    {
        return;
    }
    // storedToken() is 0 when there is no record yet, which is exactly
    // COMMISSION_TOKEN_NONE: preserving it leaves a patched image still
    // waiting to be applied rather than marking it consumed.
    writeRecord(storedToken(), deviceTypeEffective());
}

uint16_t commissionDeviceType()
{
    // The commissioned type, or 0 when this board was never told one — the
    // caller falls back to the compile-time DEVICE_TYPE then. Read straight
    // from the AT24 record rather than cached in Settings, because it
    // describes what the factory fitted, not something the bus may change.
    if (!settingsStorageOk())
    {
        return 0;
    }
    ProvisioningRecord r;
    return readRecord(r) ? r.deviceType : 0;
}

uint32_t commissionToken(const CommissionBlock *b)
{
    return ((uint32_t)b->tokenHi << 16) | (uint32_t)b->tokenLo;
}

void commissionApplyAtBoot()
{
    // A board that cannot persist must not adopt an identity it will forget.
    // On the "AT24 acks but reads fail" path the stored blob is still intact
    // and will be used once I2C recovers, so changing the running address
    // here would make the board answer at a different slave ID after a
    // transient fault — worse than not commissioning at all.
    if (!settingsStorageOk())
    {
        return;
    }

    CommissionBlock b;
    if (!commissionRead(&b))
    {
        return;                     // ordinary build, or a damaged block
    }

    const uint32_t token = commissionToken(&b);
    if (token == COMMISSION_TOKEN_NONE || token == COMMISSION_TOKEN_ERASED)
    {
        return;                     // nobody patched this image
    }
    if (!(b.applyMask & (COMMISSION_APPLY_ID | COMMISSION_APPLY_DEVICE_TYPE)))
    {
        return;
    }

    // Apply once per patched image. Without this the block would re-apply on
    // every boot, undoing an ID legitimately changed over Modbus afterwards —
    // and a factory reset would silently come back up commissioned instead of
    // unset, because the image demanding an ID is still sitting in flash.
    if (token == storedToken())
    {
        return;
    }

    // Without FORCE, only ever fill in an ID the board does not have. 247 is
    // what every path that ends without an identity produces — a virgin
    // format, a factory reset, or a torn blob whose identifier could not be
    // salvaged. That makes a patched image physically incapable of renumbering
    // a live cabinet, which is the failure that would take a site down; FORCE
    // is the operator saying, at the tool, that they mean it.
    //
    // This guards the ID only. The device type describes what the factory
    // fitted, so re-flashing a board that already has an address must still be
    // able to correct it — gating both behind one flag would mean the only way
    // to fix a wrong type is to also authorise renumbering a live cabinet.
    bool wantId = (b.applyMask & COMMISSION_APPLY_ID) != 0;
    if (!(b.flags & COMMISSION_FLAG_FORCE) && settings().identifier != DEFAULT_IDENTIFIER)
    {
        wantId = false;
    }

    // 246 is the SET_ID switch mode's own address and 247 means "unset";
    // neither is assignable.
    if (wantId && (b.identifier < 1 || b.identifier > 245))
    {
        return;
    }

    // Values first, token last — the same discipline as the settings blob's
    // "payload first, magic last". A tear here loses only the token, and a
    // lost token means the next boot retries; the reverse order would record
    // "done" over an ID that was never written, with no retry.
    if (wantId && b.identifier != settings().identifier)
    {
        settingsEdit().identifier = b.identifier;
        if (!settingsSave())
        {
            // settingsSave() also returns false for "nothing changed", which
            // is why the guard above only lets a real change reach it: here it
            // can only mean the write failed. Leave the token unwritten so the
            // next boot tries again.
            return;
        }
    }

    // The device type rides in the same record as the token, so it lands in
    // the very write that marks this image consumed — one page write, no
    // window where the board is numbered but has forgotten what it is.
    uint16_t type = 0;
    if (b.applyMask & COMMISSION_APPLY_DEVICE_TYPE)
    {
        type = b.deviceType;
    }
    else
    {
        ProvisioningRecord existing;      // keep whatever it already knew
        if (readRecord(existing))
        {
            type = existing.deviceType;
        }
    }
    writeRecord(token, type);
}
