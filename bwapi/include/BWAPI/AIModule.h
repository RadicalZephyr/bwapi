#pragma once
#include <BWAPI/Position.h>
#include <BWAPI/Unit.h>
#include <string>

namespace BWAPI
{
  // Forward declarations
  class PlayerInterface;
  typedef PlayerInterface *Player;
  class Color;

  /// <summary>AIModule is a virtual class that is intended to be implemented or inherited by a
  /// custom AI class.</summary> The Broodwar interface is guaranteed to be initialized if any of
  /// these predefined interface functions are invoked by BWAPI.
  ///
  /// @warning 
  ///   Using BWAPI in any thread other than the thread that invokes these functions can produce
  ///   unexpected behaviour and possibly crash your bot. Multi-threaded AIs are possible so
  ///   long as all BWAPI interaction is limited to the calling thread.
  ///
  /// @note
  ///   Replays are considered games and call all of the same callbacks as a standard game would.
  ///
  /// @ingroup Interface
  class AIModule
  {
    public:
      AIModule();
      virtual ~AIModule();

      /// <summary>Called only once at the beginning of a game.</summary> It is intended that the
      /// AI module do any data initialization in this function.
      ///
      /// @warning
      ///   Using the Broodwar interface before this function is called can produce undefined
      ///   behaviour and crash your bot. (During static initialization of a class for example)
      virtual void onStart();

      /// <summary>Called once at the end of a game.</summary>
      ///
      /// <param name="isWinner">
      ///   A boolean value to determine if the current player has won the match. This value will
      ///   be true if the current player has won, and false if either the player has lost or the
      ///   game is actually a replay.
      /// </param>
      virtual void onEnd(bool isWinner);

      /// <summary>Called once for every execution of a logical frame in Broodwar.</summary>
      /// Users will generally put most of their code in this function.
      virtual void onFrame();

      /// <summary>Called when the user attempts to send a text message.</summary> This function
      /// can be used to make the bot execute text commands entered by the user for debugging
      /// purposes.
      ///
      /// <param name="text">
      ///   A string containing the exact text message that was sent by the user.
      /// </param>
      ///
      /// @note
      ///   If Flag::UserInput is disabled, then this function is not called.
      virtual void onSendText(std::string text);

      /// <summary>Called when the client receives a message from another Player.</summary> This
      /// function can be used to retrieve information from allies in team games, or just to
      /// respond to other players.
      ///
      /// <param name="player">
      ///   The Player interface object representing the owner of the text message.
      /// </param>
      /// <param name="text">
      ///   The text message that the \p player sent.
      /// </param>
      ///
      /// @note
      ///   Messages sent by the current player will never invoke this function.
      virtual void onReceiveText(Player player, std::string text);

      /// <summary>Called when a Player leaves the game.</summary> All of their units are
      /// automatically given to the neutral player with their colour and alliance parameters
      /// preserved.
      ///
      /// <param name="player">
      ///   The Player interface object representing the player that left the game.
      /// </param>
      virtual void onPlayerLeft(Player player);

      /// <summary>Called when a @Nuke has been launched somewhere on the map.</summary>
      ///
      /// <param name="target">
      ///   A Position object containing the target location of the @Nuke. If the target location
      ///   is not visible and Flag::CompleteMapInformation is disabled, then target will be
      ///   Positions::Unknown.
      /// </param>
      virtual void onNukeDetect(Position target);

      /// <summary>Called when a Unit becomes accessible.</summary>
      ///
      /// <param name="unit">
      ///   The Unit interface object representing the unit that has just become accessible.
      /// </param>
      ///
      /// @note
      ///   This function INCLUDES the state of Flag::CompleteMapInformation.
      ///
      /// @see onUnitShow
      virtual void onUnitDiscover(Unit unit);

      /// <summary>Called when a Unit becomes inaccessible.</summary>
      ///
      /// <param name="unit">
      ///   The Unit interface object representing the unit that has just become inaccessible.
      /// </param>
      ///
      /// @note
      ///   This function INCLUDES the state of Flag::CompleteMapInformation.
      ///
      /// @see onUnitHide
      virtual void onUnitEvade(Unit unit);

      /// <summary>Called when a previously invisible unit becomes visible.</summary>
      ///
      /// <param name="unit">
      ///   The Unit interface object representing the unit that has just become visible.
      /// </param>
      ///
      /// @note
      ///   This function EXCLUDES the state of Flag::CompleteMapInformation.
      ///
      /// @see onUnitDiscover
      virtual void onUnitShow(Unit unit);

      /// <summary>Called just as a visible unit is becoming invisible.</summary>
      ///
      /// <param name="unit">
      ///   The Unit interface object representing the unit that is about to go out of scope.
      /// </param>
      ///
      /// @note
      ///   This function EXCLUDES the state of Flag::CompleteMapInformation.
      ///
      /// @see onUnitEvade
      virtual void onUnitHide(Unit unit);

      /// <summary>Called when any unit is created.</summary>
      ///
      /// <param name="unit">
      ///   The Unit interface object representing the unit that has just been created.
      /// </param>
      ///
      /// @note
      ///   Due to the internal workings of Broodwar, this function excludes Zerg morphing and
      ///   the construction of structures over a @Geyser .
      ///
      /// @see onUnitMorph
      virtual void onUnitCreate(Unit unit);

      /// <summary>Called when a unit is removed from the game either through death or other means.</summary>
      ///
      /// <param name="unit">
      ///   Unit object representing the unit that has just been destroyed or otherwise completely
      ///   removed from the game.
      /// </param>
      ///
      /// @note
      ///   When a @Drone morphs into an @Extractor, the @Drone is removed from the game and the
      ///   @Geyser morphs into an @Extractor.
      ///
      /// @note
      ///   If a unit is visible and destroyed, then onUnitHide is called just before this.
      virtual void onUnitDestroy(Unit unit);

      /// <summary>Called when a unit changes its UnitType.</summary> For example, when a @Drone
      /// transforms into a @Hatchery, a @SiegeTank uses @SiegeMode, or a @Geyser receives a
      /// @Refinery.
      ///
      /// <param name="unit">
      ///   Unit object representing the unit that had its UnitType change.
      /// </param>
      ///
      /// @note
      ///   This is NOT called if the unit type changes to or from UnitTypes::Unknown.
      virtual void onUnitMorph(Unit unit);

      /// <summary>Called when a unit changes ownership.</summary> This occurs when the @Protoss
      /// ability @MindControl is used, or if a unit changes ownership in @UseMapSettings .
      ///
      /// <param name="unit">
      ///   Unit interface object pertaining to the unit that has just changed ownership.
      /// </param>
      virtual void onUnitRenegade(Unit unit);

      /// <summary>Called when the state of the Broodwar game is saved to file.</summary>
      ///
      /// <param name="gameName">
      ///   A String object containing the file name that the game was saved as.
      /// </param>
      virtual void onSaveGame(std::string gameName);

      /// <summary>Called when the state of a unit changes from incomplete to complete.</summary>
      ///
      /// <param name="unit">
      ///   The Unit object representing the Unit that has just finished training or constructing.
      /// </param>
      virtual void onUnitComplete(Unit unit);
  };

}
