#include "app/diag_control.h"
#include "config.h"
#include "util/periodic_timer.h"
#include "app/latch_control.h"
#include "app/led_control.h"
#include "svc/settings.h"
#include "svc/modbus_map.h"
#include "svc/modbus_server.h"

namespace {

bool oledOk = false;
bool sensorOk[2] = {true, true};      // assume healthy until reads say otherwise
uint8_t sensorFails[2] = {0, 0};
uint32_t bootMs = 0;

void publishHealth()
{
    uint16_t bits = 0;
    bits |= settingsStorageOk() ? (1u << 0) : 0;
    bits |= oledOk              ? (1u << 1) : 0;
    bits |= sensorOk[0]         ? (1u << 2) : 0;
    bits |= sensorOk[1]         ? (1u << 3) : 0;
    bits |= latchControlLocked()? (1u << 4) : 0;
    mbRegWrite(MB_REG_HEALTH, bits);
}

} // namespace

void diagControlInit(bool oledPresent, uint8_t functionMode)
{
    oledOk = oledPresent;
    bootMs = millis();

    // Reset cause: latch the RCC flags into reg 8, then clear them so the
    // next boot reports only its own cause. bit0 IWDG is the interesting
    // one — a nonzero count of those across the fleet means loop stalls.
    uint32_t csr = RCC->CSR;
    uint16_t cause = 0;
    if (csr & RCC_CSR_IWDGRSTF) cause |= 1u << 0;
    if (csr & RCC_CSR_SFTRSTF)  cause |= 1u << 1;
    if (csr & RCC_CSR_PWRRSTF)  cause |= 1u << 2;
    if (csr & RCC_CSR_PINRSTF)  cause |= 1u << 3;
    if (csr & RCC_CSR_WWDGRSTF) cause |= 1u << 4;
    if (csr & RCC_CSR_LPWRRSTF) cause |= 1u << 5;
    if (csr & RCC_CSR_OBLRSTF)  cause |= 1u << 6;
    RCC->CSR |= RCC_CSR_RMVF;
    mbRegWrite(MB_REG_RESET_CAUSE, cause);

    mbRegWrite(MB_REG_FUNCTION_MODE, functionMode);
    publishHealth();
}

void diagControlTick(uint32_t now)
{
    static PeriodicTimer publishTimer{DIAG_PUBLISH_INTERVAL_MS};
    if (!publishTimer.due(now))
    {
        return;
    }

    uint32_t uptimeS = (now - bootMs) / 1000;
    mbRegWrite(MB_REG_UPTIME_HI, (uint16_t)(uptimeS >> 16));
    mbRegWrite(MB_REG_UPTIME_LO, (uint16_t)uptimeS);
    mbRegWrite(MB_REG_ACTIVE_PRESET, ledControlActivePreset());
    publishHealth();
}

void diagReportSensor(uint8_t idx, bool ok)
{
    if (idx > 1)
    {
        return;
    }
    if (ok)
    {
        sensorFails[idx] = 0;
        sensorOk[idx] = true; // the fresh reading overwrites any sentinel
        return;
    }
    if (sensorFails[idx] < TEMP_FAIL_THRESHOLD)
    {
        sensorFails[idx]++;
    }
    if (sensorFails[idx] >= TEMP_FAIL_THRESHOLD && sensorOk[idx])
    {
        // Sensor is gone: stop showing its stale last value — publish the
        // sentinel so a master can tell "broken" from "cold room".
        sensorOk[idx] = false;
        mbRegWrite(idx == 0 ? MB_REG_ROOM_TEMP : MB_REG_BOARD_TEMP, TEMP_SENTINEL);
    }
}

void diagSetBootCount(uint16_t count)
{
    mbRegWrite(MB_REG_BOOT_COUNT, count);
}
