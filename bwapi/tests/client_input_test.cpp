// The trusted side's answers to what the client wrote. See BWAPI/Source/BWAPI/ClientInput.h.

#include "ClientInput.h"
#include "check.h"

#include <climits>
#include <cstring>
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

  void terminateAlwaysLeavesAReadableString()
  {
    // The client can fill every byte of a fixed-size array. Everything on the trusted side then
    // reads past it: printf, sendTextEx and setMap all take a const char*.
    char full[8];
    for (char &c : full)
      c = 'x';
    const char *text = terminate(full, sizeof(full));
    CHECK_EQ(std::strlen(text), 7u);

    // An already-terminated string keeps its content.
    char normal[8] = "abc";
    CHECK_EQ(std::strcmp(terminate(normal, sizeof(normal)), "abc"), 0);

    // Degenerate inputs answer rather than fault.
    CHECK_EQ(std::strcmp(terminate(nullptr, 8), ""), 0);
    CHECK_EQ(std::strcmp(terminate(normal, 0), ""), 0);
  }

  void reserveSlotStopsAtCapacity()
  {
    int count = 0;
    CHECK_EQ(reserveSlot(count, 3), 0);
    CHECK_EQ(reserveSlot(count, 3), 1);
    CHECK_EQ(reserveSlot(count, 3), 2);
    CHECK_EQ(count, 3);

    // Full: no slot, and the count does not run past the array.
    CHECK_EQ(reserveSlot(count, 3), -1);
    CHECK_EQ(count, 3);

    // The count is shared with the client and reset every frame, so it can arrive as anything.
    int hostile = INT_MAX;
    CHECK_EQ(reserveSlot(hostile, 3), -1);
    CHECK_EQ(hostile, INT_MAX);

    int negative = -5;
    CHECK_EQ(reserveSlot(negative, 3), 0);
    CHECK_EQ(negative, 1);

    int zeroCapacity = 0;
    CHECK_EQ(reserveSlot(zeroCapacity, 0), -1);
  }

  // Every slot reserveSlot hands out is a slot indexInRange accepts.
  void reserveAndIndexAgree()
  {
    constexpr int capacity = 5;
    int count = -2;
    for (int i = 0; i < 20; ++i)
    {
      const int slot = reserveSlot(count, capacity);
      if (slot < 0)
        CHECK(count == capacity);
      else
        CHECK(indexInRange(slot, capacity));
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
  terminateAlwaysLeavesAReadableString();
  reserveSlotStopsAtCapacity();
  reserveAndIndexAgree();
  TEST_MAIN_EPILOGUE();
}
