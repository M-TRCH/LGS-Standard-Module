#ifndef UTIL_PERIODIC_TIMER_H
#define UTIL_PERIODIC_TIMER_H

#include <Arduino.h>

/*  @file util/periodic_timer.h
 *  @brief Millis-based interval helper for cooperative (non-blocking) tasks.
 *
 *  Usage:
 *      static PeriodicTimer timer{1000};
 *      if (timer.due(millis())) { ... }   // true once per period
 *
 *  Rollover-safe: uses elapsed-subtraction (now - last), never absolute
 *  deadlines, so it keeps working across the millis() wrap at ~49.7 days.
 */
struct PeriodicTimer
{
    uint32_t period;    // interval in milliseconds
    uint32_t last = 0;  // timestamp of the last firing

    bool due(uint32_t now)
    {
        if (now - last >= period)
        {
            last = now;
            return true;
        }
        return false;
    }
};

#endif // UTIL_PERIODIC_TIMER_H
