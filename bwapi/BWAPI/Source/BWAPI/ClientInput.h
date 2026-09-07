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
  }
}
