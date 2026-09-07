// The trusted side's answers to what the client wrote. See BWAPI/Source/BWAPI/ClientInput.h.

#include "ClientInput.h"
#include "check.h"

#include <climits>
#include <initializer_list>

using namespace BWAPI::ClientInput;

namespace
{
  void indexInRangeAcceptsOnlyValidSubscripts()
  {
    CHECK(indexInRange(0, 1));
    CHECK(indexInRange(4, 5));
    CHECK(!indexInRange(5, 5));
    CHECK(!indexInRange(-1, 5));

    // An empty container admits nothing, including zero. Server.cpp reaches this every frame
    // before any unit has been issued a handle.
    CHECK(!indexInRange(0, 0));
    CHECK(!indexInRange(-1, 0));

    // The client writes ints, so the extremes are reachable input rather than theory.
    CHECK(!indexInRange(INT_MAX, 5));
    CHECK(!indexInRange(INT_MIN, 5));
    CHECK(indexInRange(INT_MAX - 1, INT_MAX));

    // A negative count cannot come from .size(), but it must not admit an index either.
    CHECK(!indexInRange(0, -1));
    CHECK(!indexInRange(-1, -1));
  }

  void clampCountTruncatesRatherThanTrusts()
  {
    CHECK_EQ(clampCount(0, 20000), 0);
    CHECK_EQ(clampCount(17, 20000), 17);
    CHECK_EQ(clampCount(20000, 20000), 20000);

    // The defect: a client-supplied count used as a loop bound over a fixed array.
    CHECK_EQ(clampCount(20001, 20000), 20000);
    CHECK_EQ(clampCount(INT_MAX, 20000), 20000);

    // A negative count means there is nothing to read, not that the loop runs backwards.
    CHECK_EQ(clampCount(-1, 20000), 0);
    CHECK_EQ(clampCount(INT_MIN, 20000), 0);

    // A zero capacity clamps everything away.
    CHECK_EQ(clampCount(5, 0), 0);
    CHECK_EQ(clampCount(INT_MAX, 0), 0);
  }

  // The result of a clamp is always a valid loop bound: every index it admits is in range.
  void clampAndIndexAgree()
  {
    constexpr int capacity = 8;
    for (int written : {INT_MIN, -3, 0, 1, 7, 8, 9, INT_MAX})
    {
      const int bound = clampCount(written, capacity);
      CHECK(bound >= 0 && bound <= capacity);
      for (int i = 0; i < bound; ++i)
        CHECK(indexInRange(i, capacity));
    }
  }

  // Both are usable in constant expressions, so a bound can be checked at compile time where
  // the capacity is a constant.
  static_assert(indexInRange(0, 1), "");
  static_assert(!indexInRange(1, 1), "");
  static_assert(clampCount(INT_MAX, 20000) == 20000, "");
  static_assert(clampCount(-1, 20000) == 0, "");
}

int main()
{
  indexInRangeAcceptsOnlyValidSubscripts();
  clampCountTruncatesRatherThanTrusts();
  clampAndIndexAgree();
  TEST_MAIN_EPILOGUE();
}
