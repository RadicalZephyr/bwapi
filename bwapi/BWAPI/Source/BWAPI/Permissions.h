#pragma once

// What a bot is allowed to ask the game to do.
//
// BWAPI's thirteen Tournament::ActionID values name every call a bot can make that changes
// something other than its own units: the cheat flags, the game speed, the frame skip, the GUI,
// pause and resume, the map, and the chat. Upstream gates them behind TournamentModule::onAction,
// a veto implemented by a DLL loaded into the bot's own address space - which is defect 2.4 in
// ADR 0001 section 2, because an enforcement point a bot shares an address space with is not an
// enforcement point.
//
// The veto is replaced by this: a table the game process reads from bwapi.ini and a switch that
// refuses. That is not a referee - a config file is still inside the trust domain the bot's game
// shares - but it is strictly better than what it replaces, because nothing the bot loads can
// reach it.
//
// The defaults reproduce ExampleTournamentModule's policy exactly, which is the only stated
// tournament posture in this tree: the cheat flags, pause, resume, frame skip, GUI, local speed
// and map are refused; leaving the game, latency compensation, text size and chat are allowed;
// and command optimisation is allowed at or above a floor rather than refused, because refusing
// it leaves a bot at level 0, which is the outcome the reference module's floor exists to
// prevent.
//
// The table is asked where a request *arrives* - Server::processCommands for the client's
// commands, GameImpl::parseText for a command typed into the game - and never inside the GameImpl
// method that carries the request out. Those methods are also how BWAPI does its own housekeeping:
// initializeData resets the frame skip and the GUI flag at every match start, setGUI sets the frame
// skip, and the drawing code changes the text size several times a frame. Asking there denied BWAPI
// its own resets, silently and by default, which is a defect this file shipped with until the
// shadow-StarCraft harness caught the frame skip not being reset at match start.
//
// Nothing here includes <windows.h> or reads a file: PermissionTable is plain data with a pure
// decision function, so tests/permissions_test.cpp can ask it every question without a game. The
// ini reading lives in Config.cpp.

#include <BWAPI/Flag.h>
#include <BWAPI/TournamentAction.h>

namespace BWAPI
{
  /// The number of Tournament::ActionID values. The enum has no terminator of its own, and this
  /// header is where a new action would have to be given a default, so the count lives here.
  constexpr int TOURNAMENT_ACTION_COUNT = Tournament::SetCommandOptimizationLevel + 1;

  /// A static policy: one verdict per action, plus the one threshold the reference policy needs.
  struct PermissionTable
  {
    /// Indexed by Tournament::ActionID.
    bool allowed[TOURNAMENT_ACTION_COUNT];

    /// The lowest command optimisation level a bot may select, when SetCommandOptimizationLevel
    /// is allowed at all. ExampleTournamentModule uses 1 "to reduce APM with no action loss";
    /// the default here is 0, so an ungoverned game behaves as it does today and an operator
    /// raises the floor deliberately.
    int minCommandOptimization;

    /// The policy that applies when bwapi.ini says nothing.
    static constexpr PermissionTable defaults() noexcept
    {
      PermissionTable t{};
      // Refused: each of these changes the game rather than the bot.
      t.allowed[Tournament::EnableFlag]    = false;  // both flags are cheats; Flag::Max is 2
      t.allowed[Tournament::PauseGame]     = false;
      t.allowed[Tournament::ResumeGame]    = false;
      t.allowed[Tournament::SetLocalSpeed] = false;
      t.allowed[Tournament::SetFrameSkip]  = false;
      t.allowed[Tournament::SetGUI]        = false;
      t.allowed[Tournament::SetMap]        = false;

      // Allowed: resigning is legitimate play, and the rest are cosmetic or bot-local.
      t.allowed[Tournament::LeaveGame]   = true;
      t.allowed[Tournament::SetLatCom]   = true;
      t.allowed[Tournament::SetTextSize] = true;
      t.allowed[Tournament::Printf]      = true;
      t.allowed[Tournament::SendText]    = true;

      // Allowed above a floor, which defaults to no floor.
      t.allowed[Tournament::SetCommandOptimizationLevel] = true;
      t.minCommandOptimization = 0;
      return t;
    }

    /// May the bot perform \p action?
    ///
    /// \p parameter is the same pointer BWAPI passes to TournamentModule::onAction: an int* for
    /// EnableFlag and SetCommandOptimizationLevel, unread otherwise. A null parameter where one
    /// is required is refused rather than dereferenced, because the caller is the only thing
    /// that knows which actions carry one and this must be total.
    constexpr bool permits(Tournament::ActionID action, const void *parameter) const noexcept
    {
      const int index = static_cast<int>(action);
      if (index < 0 || index >= TOURNAMENT_ACTION_COUNT)
        return false;
      if (!allowed[index])
        return false;

      if (action == Tournament::SetCommandOptimizationLevel)
      {
        if (!parameter)
          return false;
        return *static_cast<const int *>(parameter) >= minCommandOptimization;
      }
      return true;
    }
  };

  /// The table this process is running under, read once from bwapi.ini. Defined in Config.cpp.
  ///
  /// Read once and cached: policy that could be re-read mid-game is policy a bot with a
  /// filesystem could rewrite.
  const PermissionTable &permissions();
}
