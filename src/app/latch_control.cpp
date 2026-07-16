#include "app/latch_control.h"
#include "config.h"
#include "app/led_control.h"
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
uint32_t pulseMinMs = 0;        // minimum ON time for this request (extends to the 500ms cap while locked)
uint16_t pendingCoil = 0;       // coil to clear when the request resolves
bool pendingEnableSync = false; // set the LED enable coil on completion
bool pendingIgnoreSense = false;// skip sense checks: always pulse, fixed pulseMinMs (bench test)

// Debounced lock state: two consecutive LOW samples (one per loop tick)
bool lockedDebounced = false;
bool lastSenseSample = false;

uint32_t lastTimeLatchLocked = 0;

// Resolve the in-flight request: clear its coil and sync the enable coil
// when the LED-latch command asked for it. The sync is skipped if the LED
// was legitimately turned off while the request was in flight (max-on-time
// enforcement or a bus write) — otherwise coil 1001 would read 1 with the
// ring dark and a later write-1 would be shadow-suppressed.
void finishRequest()
{
    if (pendingCoil != 0)
    {
        mbCoilWrite(pendingCoil, false);
    }
    if (pendingEnableSync && ledControlChannelOn())
    {
        mbCoilWrite(MB_COIL_LED_1_ENABLE, true);
    }
    pendingCoil = 0;
    pendingEnableSync = false;
}

// Safety trigger coil (1020): sense-aware — pulses only if the latch reads
// locked, at least LATCH_PULSE_MS, extending while still locked, 500ms cap.
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

// Force trigger coil (1019): ignores the sense pin entirely and always fires
// a fixed full-width pulse at the solenoid spec max (LATCH_MAX_UNLOCK_TIME).
// The 500ms hard cap (exit + TIM7 guard) and 2s cooldown still apply.
void onLatchForceTriggerCommand(uint16_t addr, uint16_t value)
{
    (void)addr;
    (void)value;

    if (latchBusyWith(MB_COIL_LATCH_FORCE_TRIGGER))
    {
        return;
    }
    if (!latchRequestUnlock(LATCH_MAX_UNLOCK_TIME, MB_COIL_LATCH_FORCE_TRIGGER, false,
                            /*ignoreSense=*/true))
    {
        mbCoilWrite(MB_COIL_LATCH_FORCE_TRIGGER, false);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void latchControlInit()
{
    mbRegisterHandler(MB_WATCH_COIL_COMMAND, MB_COIL_LATCH_TRIGGER, onLatchTriggerCommand);
    mbRegisterHandler(MB_WATCH_COIL_COMMAND, MB_COIL_LATCH_FORCE_TRIGGER, onLatchForceTriggerCommand);
}

bool latchRequestUnlock(uint16_t pulseMs, uint16_t coilToClear, bool syncEnableCoil,
                        bool ignoreSense)
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

    pulseMinMs = pulseMs;
    if (pulseMinMs > LATCH_MAX_UNLOCK_TIME)
    {
        pulseMinMs = LATCH_MAX_UNLOCK_TIME; // the minimum can never exceed the hard cap
    }

    pendingCoil = coilToClear;
    pendingEnableSync = syncEnableCoil;
    pendingIgnoreSense = ignoreSense;
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
                // Normal: only pulse if the latch reads locked. ignoreSense
                // (bench test) always pulses regardless of the sense pin.
                if (pendingIgnoreSense || boardLatchSenseLow())
                {
                    pulseStartMs = now;
                    boardLatchMosfetSet(true);
                    // Hardware backstop: a timer ISR forces the MOSFET low at
                    // the hard cap even if the loop stalls inside a long Modbus
                    // poll — the 500ms solenoid limit must not depend on tick
                    // cadence. Armed at the cap (not the minimum) because the
                    // pulse may legitimately extend to 500ms while still locked.
                    boardLatchGuardArm(LATCH_MAX_UNLOCK_TIME);
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
        {
            // Energize for at least pulseMinMs (300ms) regardless of sense;
            // after that keep energizing while the latch is still locked so a
            // slow latch gets more drive, but never past the 500ms hard cap.
            // Checked every tick, with the armed hardware guard as the
            // stall-proof backstop at the cap.
            // ignoreSense (bench test): run a fixed pulse to the cap, no early
            // exit. Normal: min pulseMinMs, extend while locked, cap at 500ms.
            uint32_t elapsed = now - pulseStartMs;
            if (elapsed >= LATCH_MAX_UNLOCK_TIME ||
                (!pendingIgnoreSense && elapsed >= pulseMinMs && !boardLatchSenseLow()))
            {
                boardLatchGuardDisarm();
                boardLatchMosfetSet(false);
                finishRequest();
                state = LatchState::COOLDOWN;
            }
            break;
        }

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
