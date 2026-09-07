#pragma once

namespace BWAPI
{
  /// <summary>The actions a bot may be allowed or denied: every call that changes something
  /// other than the bot's own units.</summary>
  ///
  /// Each value names one key in the [permissions] section of bwapi.ini, which the game process
  /// reads once at startup. The name is historical - these were once vetted by a tournament
  /// module loaded into the bot's own process, which is why they are enumerated at all.
  namespace Tournament
  {
    /// <summary>The actions a bot may be allowed or denied.</summary>
    enum ActionID
    {
      /// @see Game::enableFlag
      EnableFlag,

      /// @see Game::pauseGame
      PauseGame,
      
      /// @see Game::resumeGame
      ResumeGame,

      /// @see Game::leaveGame
      LeaveGame,

      /// @see Game::setLocalSpeed
      SetLocalSpeed,

      /// @see Game::setTextSize
      SetTextSize,

      /// @see Game::setLatCom
      SetLatCom,

      /// @see Game::setGUI
      SetGUI,

      /// @see Game::setMap
      SetMap,

      /// @see Game::setFrameSkip
      SetFrameSkip,

      /// @see Game::printf
      Printf,

      /// @see Game::sendText
      SendText,

      /// @see Game::setCommandOptimizationLevel
      SetCommandOptimizationLevel
    };

  };
};
