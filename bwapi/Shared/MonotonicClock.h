#pragma once

// A monotonic microsecond clock, and the only one this library measures a bot with.
//
// Defect 2.5 in ADR 0001 section 2: the per-frame meter's clock is GetTickCount, whose
// granularity is 10-16 ms, measured against thresholds of 42 to 55 ms. That is a resolution of
// three or four ticks across the whole budget, so a frame genuinely at 45 ms can be measured over
// a 55 ms line and a bot sitting near the threshold is charged for measurement noise. The unit
// stays wall clock (ADR decision 2); only the clock is wrong.
//
// QueryPerformanceCounter is monotonic, is not affected by the system clock being set, and has
// sub-microsecond resolution on every Windows this library targets. Wine implements it against
// the host's monotonic clock, which is the substrate ADR decision 9 chose.
//
// The tick-to-microsecond conversion is separated out because it is the part that can be wrong
// without anyone noticing: the obvious spelling overflows, and it does so only after the machine
// has been up for a while. tests/monotonic_clock_test.cpp pins it.

#if defined(_WIN32)
#include <windows.h>
#else
#include <ctime>
#endif

namespace BWAPI
{
  namespace Clock
  {
    /// Convert performance-counter ticks to microseconds without overflowing.
    ///
    /// `ticks * 1000000 / frequency` is the natural spelling and it is wrong: QPC counts from
    /// boot, so on a 10 MHz counter the product passes 2^63 after about ten and a half days of
    /// uptime and the result goes negative. Splitting into whole seconds plus a remainder keeps
    /// both factors small, and the remainder is strictly below the frequency by construction.
    constexpr long long ticksToMicros(long long ticks, long long frequency) noexcept
    {
      if (frequency <= 0)
        return 0;
      const long long seconds = ticks / frequency;
      const long long remainder = ticks % frequency;
      return seconds * 1000000LL + (remainder * 1000000LL) / frequency;
    }

    /// Microseconds from a monotonic source. The epoch is arbitrary; only differences mean
    /// anything, and they are meaningful across processes because both sources are system-wide.
    ///
    /// The POSIX arm is not decoration: the client is built and driven on Linux by the test
    /// substrate this library's consumers use, so a Windows-only clock here would mean a
    /// Windows-only client.
    inline long long micros() noexcept
    {
#if defined(_WIN32)
      static const long long frequency = [] {
        LARGE_INTEGER f;
        return QueryPerformanceFrequency(&f) ? f.QuadPart : 0LL;
      }();

      LARGE_INTEGER now;
      if (frequency <= 0 || !QueryPerformanceCounter(&now))
        return 0;
      return ticksToMicros(now.QuadPart, frequency);
#else
      timespec now{};
      if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
      return static_cast<long long>(now.tv_sec) * 1000000LL + now.tv_nsec / 1000;
#endif
    }

    /// The same instant in milliseconds, for the few fields that are a DWORD and must stay one.
    inline unsigned long millis() noexcept
    {
      return static_cast<unsigned long>(micros() / 1000);
    }
  }
}
