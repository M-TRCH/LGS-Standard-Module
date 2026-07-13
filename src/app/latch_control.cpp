#include "app/latch_control.h"
#include "config.h"
#include "drivers/board_io.h"
#include "svc/modbus_map.h"
#include "svc/modbus_server.h"

// ---------------------------------------------------------------------------
// Pulse state machine
// ---------------------------------------------------------------------------

namespace {

enum class LatchState : uint8_t
{
    IDLE,       // ready for a request
    DELAY,      // configured pre-unlock delay running
    PULSE,      // MOSFET energized
    COOLDOWN,   // enforcing LATCH_MIN_INTERVAL (from pulse start)
};

LatchState state = LatchState::IDLE;
uint32_t phaseStartMs = 0;      // DELAY phase reference
uint32_t pulseStartMs = 0;      // PULSE start; also the COOLDOWN reference
uint32_t delayMs = 0;           // captured unlock delay for this request
uint32_t pulseWidthMs = 0;      // clamped pulse width for this request
uint16_t pendingCoil = 0;       // coil to clear when the request resolves
bool pendingEnableSync = false; // set the LED enable coil on completion

// Debounced lock state: two consecutive LOW samples (one per loop tick)
bool lockedDebounced = false;
bool lastSenseSample = false;

uint32_t lastTimeLatchLocked = 0;

// Resolve the in-flight request: clear its coil and sync the enable coil
// when the LED-latch command asked for it.
void finishRequest()
{
    if (pendingCoil != 0)
    {
        mbCoilWrite(pendingCoil, false);
    }
    if (pendingEnableSync)
    {
        mbCoilWrite(MB_COIL_LED_1_ENABLE, true);
    }
    pendingCoil = 0;
    pendingEnableSync = false;
}

// Latch trigger coil (1020)
void onLatchTriggerCommand(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;

    if (latchBusyWith(MB_COIL_LATCH_TRIGGER))
    {
        return; // request in flight: coil stays set until the pulse resolves
    }
    if (!latchRequestUnlock(LATCH_PULSE_MS, MB_COIL_LATCH_TRIGGER, false))
    {
        // Busy (delay/pulse/cooldown of another request): reject and clear
        // immediately, otherwise the command would retry every poll and
        // fire a surprise unlock the moment the cooldown expires.
        mbCoilWrite(MB_COIL_LATCH_TRIGGER, false);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void latchControlInit()
{
    mbRegisterHandler(MB_WATCH_COIL_COMMAND, MB_COIL_LATCH_TRIGGER, onLatchTriggerCommand);
}

bool latchRequestUnlock(uint16_t pulseMs, uint16_t coilToClear, bool syncEnableCoil)
{
    if (state != LatchState::IDLE)
    {
        return false;
    }

    delayMs = mbRegRead(MB_REG_UNLOCK_DELAY);
    if (delayMs > UNLOCK_DELAY_MAX_MS)
    {
        delayMs = UNLOCK_DELAY_MAX_MS;
    }

    pulseWidthMs = pulseMs;
    if (pulseWidthMs > LATCH_MAX_UNLOCK_TIME)
    {
        pulseWidthMs = LATCH_MAX_UNLOCK_TIME;
    }

    pendingCoil = coilToClear;
    pendingEnableSync = syncEnableCoil;
    phaseStartMs = millis();
    state = LatchState::DELAY;
    return true;
}

bool latchBusyWith(uint16_t coil)
{
    return (state != LatchState::IDLE) && (pendingCoil == coil);
}

void latchControlTick(uint32_t now)
{
    switch (state)
    {
        case LatchState::DELAY:
            if (now - phaseStartMs >= delayMs)
            {
                if (boardLatchSenseLow())
                {
                    pulseStartMs = now;
                    boardLatchMosfetSet(true);
                    state = LatchState::PULSE;
                }
                else
                {
                    // Latch not present: no pulse and no cooldown consumed,
                    // matching the original unlockLatch() returning false.
                    finishRequest();
                    state = LatchState::IDLE;
                }
            }
            break;

        case LatchState::PULSE:
            // End the pulse when the latch releases (sense goes high) or the
            // clamped width expires — checked every tick.
            if (!boardLatchSenseLow() || (now - pulseStartMs >= pulseWidthMs))
            {
                boardLatchMosfetSet(false);
                finishRequest();
                state = LatchState::COOLDOWN;
            }
            break;

        case LatchState::COOLDOWN:
            if (now - pulseStartMs >= LATCH_MIN_INTERVAL)
            {
                state = LatchState::IDLE;
            }
            break;

        case LatchState::IDLE:
        default:
            break;
    }

    // Structural safety invariant: outside PULSE the MOSFET is driven low
    // every tick, so a missed transition can never leave the gate energized.
    if (state != LatchState::PULSE)
    {
        boardLatchMosfetSet(false);
    }

    // Debounced lock state (two consecutive loop samples)
    bool sample = boardLatchSenseLow();
    lockedDebounced = sample && lastSenseSample;
    lastSenseSample = sample;

    // Time after unlocking (reg 40): 0 while locked, seconds since the
    // latch was last seen locked otherwise.
    if (lockedDebounced)
    {
        lastTimeLatchLocked = now;
        mbRegWrite(MB_REG_TIME_AFTER_UNLOCK, 0);
    }
    else
    {
        mbRegWrite(MB_REG_TIME_AFTER_UNLOCK, (uint16_t)((now - lastTimeLatchLocked) / 1000));
    }
}
