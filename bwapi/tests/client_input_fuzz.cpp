// A million hostile command planes through the trusted side's consume loops.
//
// The validators in ClientInput.h are total by construction and client_input_test.cpp checks
// each one against the extremes. What that cannot show is whether the *call sites* use them
// correctly: Server::processCommands and GameImpl::drawShapes each clamp a count, then index
// several arrays with what the client wrote, and getting one of those wrong is the defect.
//
// So this models the plane rather than the functions. It builds the same shape at small sizes -
// small enough that an off-by-one runs off the end rather than into the rest of a 33 MB struct -
// fills the counts and indices with values chosen to be hostile, and runs the same consume loops
// the server runs. Under ASan the arrays are poisoned at their real bounds, so a missing check is
// a diagnostic rather than a silent read of the next field.
//
// It also counts what it generated and fails if the corpus was not adversarial: a fuzzer that
// only ever produces valid input passes for the wrong reason.

#include "ClientInput.h"
#include "check.h"

#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace BWAPI::ClientInput;

namespace
{
  // Deliberately tiny. The real arrays are 20,000 entries; at that size an unclamped loop reads
  // adjacent members of the same struct and ASan sees nothing.
  constexpr int MAX_STRINGS = 4;
  constexpr int STRING_CAPACITY = 8;
  constexpr int MAX_SHAPES = 4;
  constexpr int MAX_COMMANDS = 4;

  // The client-written half of GameData, in miniature. Heap-allocated per iteration so ASan
  // brackets every array with redzones.
  struct CommandPlane
  {
    char strings[MAX_STRINGS][STRING_CAPACITY];
    int stringCount;

    struct { int type; int stringIndex; } shapes[MAX_SHAPES];
    int shapeCount;

    struct { int type; int value1; int value2; } commands[MAX_COMMANDS];
    int commandCount;
  };

  struct Rng
  {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 1) {}
    uint64_t next()
    {
      s ^= s << 13; s ^= s >> 7; s ^= s << 17;
      return s;
    }
    // A count or index the client might write: mostly boundary values, some noise.
    int hostileInt()
    {
      switch (next() % 12)
      {
      case 0:  return INT_MAX;
      case 1:  return INT_MIN;
      case 2:  return -1;
      case 3:  return 0;
      case 4:  return MAX_STRINGS;
      case 5:  return MAX_STRINGS - 1;
      case 6:  return MAX_SHAPES + 1;
      case 7:  return MAX_COMMANDS;
      case 8:  return static_cast<int>(next() % 32) - 8;
      case 9:  return static_cast<int>(next());
      case 10: return -static_cast<int>(next() % 1000000);
      default: return static_cast<int>(next() % 8);
      }
    }
  };

  struct Corpus
  {
    long outOfRangeCounts = 0;
    long outOfRangeIndices = 0;
    long unterminatedStrings = 0;
  };

  void fill(CommandPlane &p, Rng &rng, Corpus &corpus)
  {
    for (int i = 0; i < MAX_STRINGS; ++i)
    {
      // Half the strings are filled to the brim with no terminator, which is what a client that
      // writes STRING_CAPACITY bytes leaves behind.
      if (rng.next() % 2)
      {
        std::memset(p.strings[i], 'x', STRING_CAPACITY);
        ++corpus.unterminatedStrings;
      }
      else
      {
        std::memset(p.strings[i], 0, STRING_CAPACITY);
        p.strings[i][0] = 'a';
      }
    }
    p.stringCount = rng.hostileInt();
    p.shapeCount = rng.hostileInt();
    p.commandCount = rng.hostileInt();

    if (p.stringCount < 0 || p.stringCount > MAX_STRINGS) ++corpus.outOfRangeCounts;
    if (p.shapeCount < 0 || p.shapeCount > MAX_SHAPES) ++corpus.outOfRangeCounts;
    if (p.commandCount < 0 || p.commandCount > MAX_COMMANDS) ++corpus.outOfRangeCounts;

    for (int i = 0; i < MAX_SHAPES; ++i)
    {
      p.shapes[i].type = static_cast<int>(rng.next() % 3);
      p.shapes[i].stringIndex = rng.hostileInt();
      if (!indexInRange(p.shapes[i].stringIndex, MAX_STRINGS)) ++corpus.outOfRangeIndices;
    }
    for (int i = 0; i < MAX_COMMANDS; ++i)
    {
      p.commands[i].type = static_cast<int>(rng.next() % 5);
      p.commands[i].value1 = rng.hostileInt();
      p.commands[i].value2 = rng.hostileInt();
      if (!indexInRange(p.commands[i].value1, MAX_STRINGS)) ++corpus.outOfRangeIndices;
    }
  }

  // Server::processCommands, in miniature: clamp the count, then resolve string indices through
  // the clamped count and read every byte of what comes back.
  size_t consumeCommands(CommandPlane &p)
  {
    const int stringCount = clampCount(p.stringCount, MAX_STRINGS);
    const int commandCount = clampCount(p.commandCount, MAX_COMMANDS);

    const auto clientString = [&](int index) -> const char * {
      if (!indexInRange(index, stringCount))
        return "";
      return terminate(p.strings[index], STRING_CAPACITY);
    };

    size_t consumed = 0;
    for (int i = 0; i < commandCount; ++i)
    {
      switch (p.commands[i].type)
      {
      case 0:  // Printf
      case 1:  // SendText
      case 2:  // SetMap
        consumed += std::strlen(clientString(p.commands[i].value1));
        break;
      default:
        break;
      }
    }
    return consumed;
  }

  // GameImpl::drawShapes, in miniature.
  size_t consumeShapes(CommandPlane &p)
  {
    const int stringCount = clampCount(p.stringCount, MAX_STRINGS);
    const int shapeCount = clampCount(p.shapeCount, MAX_SHAPES);

    size_t consumed = 0;
    for (int i = 0; i < shapeCount; ++i)
    {
      if (p.shapes[i].type != 0)  // only Text carries a string index
        continue;
      const int index = p.shapes[i].stringIndex;
      const char *text = indexInRange(index, stringCount)
        ? terminate(p.strings[index], STRING_CAPACITY)
        : "";
      consumed += std::strlen(text);
    }
    return consumed;
  }

  // GameImpl::addShape and addString, in miniature: the trusted side appending into a plane whose
  // count the client just wrote.
  size_t appendAfterClient(CommandPlane &p)
  {
    size_t appended = 0;
    for (int i = 0; i < 6; ++i)
    {
      const int shapeSlot = reserveSlot(p.shapeCount, MAX_SHAPES);
      if (shapeSlot >= 0)
      {
        p.shapes[shapeSlot].type = 0;
        p.shapes[shapeSlot].stringIndex = -1;
        ++appended;
      }
      const int stringSlot = reserveSlot(p.stringCount, MAX_STRINGS);
      if (stringSlot >= 0)
      {
        std::memset(p.strings[stringSlot], 0, STRING_CAPACITY);
        p.strings[stringSlot][0] = 'z';
        ++appended;
      }
    }
    return appended;
  }
}

int main(int argc, char **argv)
{
  const long iterations = argc > 1 ? std::strtol(argv[1], nullptr, 10) : 1000000;
  const uint64_t seed = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 0x5eed1234u;

  Rng rng(seed);
  Corpus corpus;
  size_t sink = 0;

  for (long i = 0; i < iterations; ++i)
  {
    // A fresh allocation each time so ASan's redzones sit immediately past every array.
    CommandPlane *p = new CommandPlane();
    fill(*p, rng, corpus);
    sink += consumeCommands(*p);
    sink += consumeShapes(*p);
    sink += appendAfterClient(*p);
    // Whatever the client wrote, the counts are now within the arrays they describe.
    CHECK(p->shapeCount >= 0 && p->shapeCount <= MAX_SHAPES);
    CHECK(p->stringCount >= 0 && p->stringCount <= MAX_STRINGS);
    delete p;
  }

  // The corpus has to have been adversarial, or the run above proves nothing.
  CHECK(corpus.outOfRangeCounts > iterations / 10);
  CHECK(corpus.outOfRangeIndices > iterations / 10);
  CHECK(corpus.unterminatedStrings > iterations / 10);

  std::fprintf(stderr,
               "%ld planes: %ld out-of-range counts, %ld out-of-range indices, "
               "%ld unterminated strings (sink %zu)\n",
               iterations, corpus.outOfRangeCounts, corpus.outOfRangeIndices,
               corpus.unterminatedStrings, sink);
  TEST_MAIN_EPILOGUE();
}
