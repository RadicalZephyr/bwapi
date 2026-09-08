// The tick-to-microsecond conversion. See BWAPI/Source/BWAPI/MonotonicClock.h.
//
// QueryPerformanceCounter itself is untestable here and barely worth testing anywhere: it is one
// call. The conversion is the part that can be quietly wrong, because the natural spelling
// overflows only after the machine has been up long enough that nobody connects the two.

#include "MonotonicClock.h"
#include "check.h"

#include <climits>
#include <initializer_list>
#include <chrono>
#include <thread>

using BWAPI::Clock::ticksToMicros;

namespace
{
  void convertsTheOrdinaryCases()
  {
    // A 10 MHz counter, which is what QPC reports on most modern Windows.
    constexpr long long tenMHz = 10000000LL;
    CHECK_EQ(ticksToMicros(0, tenMHz), 0);
    CHECK_EQ(ticksToMicros(10, tenMHz), 1);            // 10 ticks = 1 us
    CHECK_EQ(ticksToMicros(tenMHz, tenMHz), 1000000);  // one second
    CHECK_EQ(ticksToMicros(tenMHz * 42 / 1000, tenMHz), 42000);  // one 42 ms frame

    // A frequency that does not divide evenly, which older hardware reports.
    constexpr long long tsc = 3579545LL;
    CHECK_EQ(ticksToMicros(tsc, tsc), 1000000);
    CHECK_EQ(ticksToMicros(tsc / 2, tsc), 499999);  // truncation, not overflow
  }

  void doesNotOverflowAfterUptime()
  {
    constexpr long long tenMHz = 10000000LL;

    // The naive `ticks * 1000000 / frequency` overflows a signed 64-bit product once ticks pass
    // roughly 9.2e12, which on a 10 MHz counter is about ten and a half days of uptime. Every
    // one of these is past that and every one has to come back positive and correct.
    for (long long days : {1LL, 11LL, 100LL, 1000LL, 10000LL})
    {
      const long long ticks = days * 86400LL * tenMHz;
      const long long micros = ticksToMicros(ticks, tenMHz);
      CHECK(micros > 0);
      CHECK_EQ(micros, days * 86400LL * 1000000LL);
    }

    // And the extreme, which must not be undefined behaviour even if the number is absurd.
    CHECK(ticksToMicros(LLONG_MAX, tenMHz) > 0);
  }

  void differencesAreExactAcrossTheOverflowPoint()
  {
    // Only differences are ever used, and the meter reports them at microsecond scale. A frame
    // measured across a region where the naive form would have overflowed must still measure the
    // frame, not garbage.
    constexpr long long tenMHz = 10000000LL;
    const long long base = 30LL * 86400LL * tenMHz;  // thirty days of uptime
    for (long long frameMicros : {1LL, 42LL, 1000LL, 42000LL, 1000000LL})
    {
      const long long after = base + frameMicros * (tenMHz / 1000000LL);
      CHECK_EQ(ticksToMicros(after, tenMHz) - ticksToMicros(base, tenMHz), frameMicros);
    }
  }

  void degenerateFrequencyAnswersRatherThanDividesByZero()
  {
    // QueryPerformanceFrequency can fail; micros() passes zero through rather than branching at
    // every call site.
    CHECK_EQ(ticksToMicros(12345, 0), 0);
    CHECK_EQ(ticksToMicros(12345, -1), 0);
  }

  // The defect this clock replaces is a resolution one: GetTickCount advances in steps of
  // 10-16 ms and the budget it was measuring against is 42 to 55, so the meter could only
  // distinguish three or four values across the whole thing. This asserts the replacement is
  // not in that class - it measures a 20 ms sleep to well inside a millisecond, and it never
  // goes backwards.
  void resolvesFarBelowATickAndNeverGoesBackwards()
  {
    long long previous = BWAPI::Clock::micros();
    CHECK(previous > 0);
    for (int i = 0; i < 10000; ++i)
    {
      const long long now = BWAPI::Clock::micros();
      CHECK(now >= previous);
      previous = now;
    }

    const long long before = BWAPI::Clock::micros();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const long long elapsed = BWAPI::Clock::micros() - before;

    // A generous window - this runs on shared CI - but one GetTickCount could not answer at all,
    // since 20 ms is between one and two of its ticks.
    CHECK(elapsed >= 19000);
    CHECK(elapsed <= 200000);

    // And the reading is not quantised to milliseconds: over ten thousand samples, at least one
    // pair differs by something that is not a whole millisecond.
    bool sawSubMillisecondStep = false;
    long long last = BWAPI::Clock::micros();
    for (int i = 0; i < 10000 && !sawSubMillisecondStep; ++i)
    {
      const long long now = BWAPI::Clock::micros();
      if (now != last && (now - last) % 1000 != 0)
        sawSubMillisecondStep = true;
      last = now;
    }
    CHECK(sawSubMillisecondStep);
  }

  // The deadline's unit conversion. Every one of these is a way a bounded wait stops being one.
  void waitMillisNeverTurnsADeadlineIntoNoDeadline()
  {
    using BWAPI::Clock::waitMillis;
    using BWAPI::Clock::WAIT_FOREVER;

    // Rounds up: a sub-millisecond budget must still wait, or the deadline expires before the
    // client has been scheduled and the meter measures the scheduler.
    CHECK_EQ(waitMillis(1), 1u);
    CHECK_EQ(waitMillis(500), 1u);
    CHECK_EQ(waitMillis(999), 1u);
    CHECK_EQ(waitMillis(1000), 1u);
    CHECK_EQ(waitMillis(1001), 2u);
    CHECK_EQ(waitMillis(42000), 42u);

    // Spent is zero, not a negative reinterpreted as four billion milliseconds.
    CHECK_EQ(waitMillis(0), 0u);
    CHECK_EQ(waitMillis(-1), 0u);
    CHECK_EQ(waitMillis(-1000000), 0u);
    CHECK_EQ(waitMillis(LLONG_MIN), 0u);

    // And an absurd budget clamps below "forever" rather than onto it.
    CHECK(waitMillis(LLONG_MAX) < WAIT_FOREVER);
    CHECK(waitMillis(LLONG_MAX) > 0);
    CHECK(waitMillis(1000LL * 1000 * WAIT_FOREVER) < WAIT_FOREVER);
  }

  static_assert(BWAPI::Clock::waitMillis(500) == 1u, "a sub-millisecond budget still waits");
  static_assert(BWAPI::Clock::waitMillis(-1) == 0u, "a spent budget does not wrap");
  static_assert(ticksToMicros(10000000LL, 10000000LL) == 1000000LL, "");
  static_assert(ticksToMicros(1, 0) == 0, "");
}

int main()
{
  convertsTheOrdinaryCases();
  doesNotOverflowAfterUptime();
  differencesAreExactAcrossTheOverflowPoint();
  degenerateFrequencyAnswersRatherThanDividesByZero();
  resolvesFarBelowATickAndNeverGoesBackwards();
  waitMillisNeverTurnsADeadlineIntoNoDeadline();
  TEST_MAIN_EPILOGUE();
}
