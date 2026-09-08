#pragma once

// Validation of everything the client writes into shared memory.
//
// The client process is untrusted: it maps GameData, writes counts and indices into it, and the
// server then uses them as loop bounds and array subscripts. Every such value has to be checked
// on this side, because the only bounds the client protocol ships today are asserts inside the
// *client* (BWAPIClient/Source/GameImpl.cpp), which NDEBUG compiles out.
//
// The checks live here, in one dependency-free header, for two reasons. They are the same three
// or four questions asked from Server.cpp and GameDrawing.cpp, and a check that is written out
// by hand at each site is a check that is eventually written out wrongly at one of them. And
// nothing in this header includes <windows.h> or reaches into BW memory, so it builds and is
// tested on any host - see tests/, which is the only automated coverage the server side has.
//
// Every function is total: it answers for any int the client can write, including negative ones
// and INT_MAX, and it never reads the memory it is describing.

namespace BWAPI
{
  namespace ClientInput
  {
    /// Is a client-supplied index a valid subscript into a container of \p count elements?
    ///
    /// The client can write any int, so both ends are checked. This is the pattern Server.cpp
    /// already applies to unitIndex and is the one every other index should have had.
    constexpr bool indexInRange(int index, int count) noexcept
    {
      return index >= 0 && index < count;
    }

    /// A client-supplied count, clamped to what the array behind it can hold.
    ///
    /// Returns a value in [0, capacity]: a negative count means the client wrote nonsense and
    /// there is nothing to read, and a count past the end is truncated rather than trusted.
    /// Callers use the result as their loop bound instead of the raw field.
    constexpr int clampCount(int count, int capacity) noexcept
    {
      if (count < 0)
        return 0;
      return count > capacity ? capacity : count;
    }

    /// Reserve the next slot of a fixed-size array, advancing \p count, or -1 if it is full.
    ///
    /// The shape and string arrays are appended to by both sides and their counts are reset by
    /// the server each frame, so an append can find a count the client wrote. Upstream guards
    /// each of these with an assert, which NDEBUG - the configuration every release ships -
    /// compiles out, leaving a write past the array.
    inline int reserveSlot(int &count, int capacity) noexcept
    {
      // Normalise first, so the count is inside the array it describes whether or not a slot was
      // available. Merely refusing would leave whatever the client wrote sitting in a field that
      // every later reader would have to remember to clamp, and one of them eventually would not.
      count = clampCount(count, capacity);
      if (count >= capacity)
        return -1;
      return count++;
    }

    /// Force a client-written character array to be NUL-terminated, and return it.
    ///
    /// GameData's string arrays are fixed-size and the client fills them itself, so it may write
    /// \p capacity non-zero bytes and leave no terminator. Everything on this side then reads
    /// past the array - Broodwar::printf, sendTextEx and setMap all take a const char*. Writing
    /// the last byte unconditionally costs one store and removes the case.
    inline const char *terminate(char *text, unsigned long long capacity) noexcept
    {
      if (!text || capacity == 0)
        return "";
      text[capacity - 1] = '\0';
      return text;
    }

  }
}
