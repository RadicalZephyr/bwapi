// Per-bot handle namespaces. See Server::issueUnitID and Server::lookupUnitID.
//
// Server itself is Windows-only and reaches into StarCraft's address space, so what runs here is
// the allocator's rule rather than the class: handles are issued only where a bot is told a unit
// exists, looked up everywhere else, and a lookup of something never issued answers -1.
//
// That rule is the whole of defect 2.7. The end-to-end property - that two world-states differing
// only in what the bot cannot see produce identical views - needs the synthetic-GameData fixture
// and is not built here; this covers the mechanism, and the distinction is deliberate.

#include "check.h"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace
{
  // Server's allocator, transcribed: the same two functions over the same two containers.
  class HandleTable
  {
  public:
    static constexpr int MAX_UNITS = 10000;

    int issue(const void *unit)
    {
      if (!unit)
        return -1;
      auto it = lookup_.find(unit);
      if (it != lookup_.end())
        return it->second;
      const int id = static_cast<int>(order_.size());
      if (id >= MAX_UNITS)
        return -1;
      lookup_[unit] = id;
      order_.push_back(unit);
      return id;
    }

    int lookup(const void *unit) const
    {
      if (!unit)
        return -1;
      auto it = lookup_.find(unit);
      return it == lookup_.end() ? -1 : it->second;
    }

    const void *unitFor(int id) const
    {
      return (id >= 0 && id < static_cast<int>(order_.size())) ? order_[id] : nullptr;
    }

    size_t size() const { return order_.size(); }

  private:
    std::unordered_map<const void *, int> lookup_;
    std::vector<const void *> order_;
  };

  // Stand-ins for units. Only their addresses matter.
  int units[64];
  const void *unit(int i) { return &units[i]; }

  void handlesAreDenseInDiscoveryOrder()
  {
    HandleTable t;
    // A bot discovers units 5, 40 and 2, in that order. The game created them in some other
    // order and created a great many others in between; none of that may be visible.
    CHECK_EQ(t.issue(unit(5)), 0);
    CHECK_EQ(t.issue(unit(40)), 1);
    CHECK_EQ(t.issue(unit(2)), 2);

    // Re-issuing is idempotent: a unit keeps its handle for the match, which is BWAPI's contract.
    CHECK_EQ(t.issue(unit(5)), 0);
    CHECK_EQ(t.issue(unit(40)), 1);
    CHECK_EQ(t.size(), 3u);
  }

  void aUnitNeverSeenHasNoHandle()
  {
    HandleTable t;
    t.issue(unit(1));
    t.issue(unit(2));

    // This is the UnitUpdate.cpp case: a visible enemy marine is attacking something the bot has
    // never seen. Upstream allocated a handle here and handed it over; the answer is that there
    // is no handle.
    CHECK_EQ(t.lookup(unit(3)), -1);
    CHECK_EQ(t.size(), 2u);   // and asking did not create one

    // A unit that has been seen keeps answering, including after it stops being accessible: the
    // bot legitimately remembers what it saw.
    CHECK_EQ(t.lookup(unit(1)), 0);
    CHECK_EQ(t.lookup(unit(2)), 1);
    CHECK_EQ(t.lookup(nullptr), -1);
    CHECK_EQ(t.issue(nullptr), -1);
  }

  // The channel, stated as a test. Two games identical from the bot's point of view but differing
  // in how much happened out of its sight must produce identical handles.
  void handlesCarryNothingAboutWhatTheBotCannotSee()
  {
    HandleTable quiet;
    HandleTable busy;

    // The bot sees the same three units in the same order in both games.
    const int seen[] = {7, 11, 3};

    for (int i = 0; i < 3; ++i)
    {
      // In the busy game, the opponent creates a hundred units between each of the bot's
      // discoveries. They are never exposed to this bot, so they are never issued a handle.
      for (int j = 0; j < 100; ++j)
        busy.lookup(unit(20 + (j % 40)));

      quiet.issue(unit(seen[i]));
      busy.issue(unit(seen[i]));
    }

    for (int i = 0; i < 3; ++i)
      CHECK_EQ(quiet.lookup(unit(seen[i])), busy.lookup(unit(seen[i])));

    CHECK_EQ(quiet.size(), busy.size());

    // And the gaps a bot could subtract are gone: consecutive discoveries are consecutive.
    CHECK_EQ(quiet.lookup(unit(11)) - quiet.lookup(unit(7)), 1);
    CHECK_EQ(busy.lookup(unit(3)) - busy.lookup(unit(11)), 1);
  }

  void issuingStopsAtTheArrayItSubscripts()
  {
    static std::vector<int> many(HandleTable::MAX_UNITS + 16);
    HandleTable t;
    for (int i = 0; i < HandleTable::MAX_UNITS; ++i)
      CHECK_EQ(t.issue(&many[i]), i);

    // data->units holds MAX_UNITS entries and the handle is the subscript, so there is no handle
    // to give past the end. Upstream kept counting and the write ran off the array.
    CHECK_EQ(t.issue(&many[HandleTable::MAX_UNITS]), -1);
    CHECK_EQ(t.issue(&many[HandleTable::MAX_UNITS + 1]), -1);
    CHECK_EQ(t.size(), static_cast<size_t>(HandleTable::MAX_UNITS));

    // Refusing does not disturb what was already issued.
    CHECK_EQ(t.lookup(&many[0]), 0);
    CHECK_EQ(t.lookup(&many[HandleTable::MAX_UNITS - 1]), HandleTable::MAX_UNITS - 1);
    CHECK_EQ(t.lookup(&many[HandleTable::MAX_UNITS]), -1);
  }

  // Every handle in circulation is a valid subscript, which is what the publishing loop needs.
  void everyIssuedHandleIsAValidSubscript()
  {
    HandleTable t;
    for (int i = 0; i < 64; ++i)
    {
      const int id = t.issue(unit(i));
      CHECK(id >= 0 && id < HandleTable::MAX_UNITS);
      CHECK(t.unitFor(id) == unit(i));
    }
    CHECK(t.unitFor(-1) == nullptr);
    CHECK(t.unitFor(64) == nullptr);
  }
}

int main()
{
  handlesAreDenseInDiscoveryOrder();
  aUnitNeverSeenHasNoHandle();
  handlesCarryNothingAboutWhatTheBotCannotSee();
  issuingStopsAtTheArrayItSubscripts();
  everyIssuedHandleIsAValidSubscript();
  TEST_MAIN_EPILOGUE();
}
