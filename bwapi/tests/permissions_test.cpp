// The policy that replaced TournamentModule::onAction. See BWAPI/Source/BWAPI/Permissions.h.
//
// The defaults are the interesting half: they are what applies when bwapi.ini says nothing, and
// they have to reproduce ExampleTournamentModule's verdicts, which are the only stated
// tournament posture in this tree.

#include "Permissions.h"
#include "check.h"

#include <climits>

using namespace BWAPI;

namespace
{
  // ExampleTournamentModule::onAction, transcribed. This is the oracle: if the defaults ever
  // stop agreeing with it, one of the two is wrong and the diff says which.
  bool referenceModuleAllows(Tournament::ActionID action, int parameter)
  {
    switch (action)
    {
    case Tournament::SendText:
    case Tournament::Printf:
      return true;
    case Tournament::EnableFlag:
      // Disallows CompleteMapInformation and UserInput; would allow any other flag, but
      // Flag::Max is 2, so there is no other flag.
      return !(parameter == Flag::CompleteMapInformation || parameter == Flag::UserInput);
    case Tournament::PauseGame:
    case Tournament::ResumeGame:
    case Tournament::SetFrameSkip:
    case Tournament::SetGUI:
    case Tournament::SetLocalSpeed:
    case Tournament::SetMap:
      return false;
    case Tournament::LeaveGame:
    case Tournament::SetLatCom:
    case Tournament::SetTextSize:
      return true;
    case Tournament::SetCommandOptimizationLevel:
      return parameter > 1;  // MINIMUM_COMMAND_OPTIMIZATION
    }
    return true;
  }

  void defaultsRefuseEverythingThatChangesTheGame()
  {
    const PermissionTable t = PermissionTable::defaults();
    int flag = Flag::CompleteMapInformation;

    CHECK(!t.permits(Tournament::EnableFlag, &flag));
    flag = Flag::UserInput;
    CHECK(!t.permits(Tournament::EnableFlag, &flag));

    CHECK(!t.permits(Tournament::PauseGame, nullptr));
    CHECK(!t.permits(Tournament::ResumeGame, nullptr));
    CHECK(!t.permits(Tournament::SetLocalSpeed, nullptr));
    CHECK(!t.permits(Tournament::SetFrameSkip, nullptr));
    CHECK(!t.permits(Tournament::SetGUI, nullptr));
    CHECK(!t.permits(Tournament::SetMap, nullptr));
  }

  void defaultsAllowLegitimatePlay()
  {
    const PermissionTable t = PermissionTable::defaults();

    // Resigning is play, not a cheat: a bot that cannot leave cannot surrender, and the
    // reference module allows it.
    CHECK(t.permits(Tournament::LeaveGame, nullptr));
    CHECK(t.permits(Tournament::SetLatCom, nullptr));
    CHECK(t.permits(Tournament::SetTextSize, nullptr));
    CHECK(t.permits(Tournament::Printf, nullptr));
    CHECK(t.permits(Tournament::SendText, nullptr));
  }

  void commandOptimizationIsAFloorNotAVeto()
  {
    PermissionTable t = PermissionTable::defaults();

    // The default floor is zero, so every level BWAPI accepts is permitted.
    for (int level = 0; level <= 4; ++level)
      CHECK(t.permits(Tournament::SetCommandOptimizationLevel, &level));

    // Raised to the reference module's floor, the low levels go.
    t.minCommandOptimization = 1;
    int level = 0;
    CHECK(!t.permits(Tournament::SetCommandOptimizationLevel, &level));
    level = 1;
    CHECK(t.permits(Tournament::SetCommandOptimizationLevel, &level));
    level = 4;
    CHECK(t.permits(Tournament::SetCommandOptimizationLevel, &level));

    // An action that needs a parameter and is handed none is refused, not dereferenced.
    CHECK(!t.permits(Tournament::SetCommandOptimizationLevel, nullptr));
  }

  void defaultsMatchTheReferenceModule()
  {
    const PermissionTable t = PermissionTable::defaults();

    for (int flag = 0; flag < Flag::Max; ++flag)
      CHECK_EQ(t.permits(Tournament::EnableFlag, &flag),
               referenceModuleAllows(Tournament::EnableFlag, flag));

    for (int i = 0; i < TOURNAMENT_ACTION_COUNT; ++i)
    {
      const auto action = static_cast<Tournament::ActionID>(i);
      if (action == Tournament::EnableFlag || action == Tournament::SetCommandOptimizationLevel)
        continue;  // parameterised; covered above
      CHECK_EQ(t.permits(action, nullptr), referenceModuleAllows(action, 0));
    }

    // SetCommandOptimizationLevel diverges by one, deliberately, because the reference module
    // is wrong. It writes `parameter > MINIMUM_COMMAND_OPTIMIZATION` with the constant at 1, so
    // it refuses level 1 - the very level its own ExampleTournamentAI::onStart asks for, and the
    // level the constant is named after. A floor of N here admits N.
    PermissionTable floored = t;
    floored.minCommandOptimization = 1;
    for (int level = 0; level <= 4; ++level)
    {
      const bool ours = floored.permits(Tournament::SetCommandOptimizationLevel, &level);
      const bool reference = referenceModuleAllows(Tournament::SetCommandOptimizationLevel, level);
      if (level == 1)
        CHECK(ours && !reference);   // the off-by-one, stated
      else
        CHECK_EQ(ours, reference);
    }
  }

  void permitsIsTotalOverGarbageActions()
  {
    const PermissionTable t = PermissionTable::defaults();
    // permissionCheck is reached from processCommands, so the action can in principle be
    // anything. Out of range is refused rather than indexed.
    CHECK(!t.permits(static_cast<Tournament::ActionID>(-1), nullptr));
    CHECK(!t.permits(static_cast<Tournament::ActionID>(TOURNAMENT_ACTION_COUNT), nullptr));
    CHECK(!t.permits(static_cast<Tournament::ActionID>(INT_MAX), nullptr));
  }

  // The table is usable at compile time, so a default can be asserted without running anything.
  static_assert(!PermissionTable::defaults().permits(Tournament::PauseGame, nullptr), "");
  static_assert(PermissionTable::defaults().permits(Tournament::LeaveGame, nullptr), "");
  static_assert(TOURNAMENT_ACTION_COUNT == 13, "a new action needs a default and an ini key");
}

int main()
{
  defaultsRefuseEverythingThatChangesTheGame();
  defaultsAllowLegitimatePlay();
  commandOptimizationIsAFloorNotAVeto();
  defaultsMatchTheReferenceModule();
  permitsIsTotalOverGarbageActions();
  TEST_MAIN_EPILOGUE();
}
