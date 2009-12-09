#include "Broodwar.h"
#include "Hook.h"
#include "Command.h"
#include "Latency.h"
#include "Offsets.h"

#include <DLLMain.h>


namespace BW
{
//public:
  //----------------------------------- IN REPLAY -----------------------------------------
  bool isInReplay()
  {
    return *(BW::BWDATA_InReplay) != 0;
  }
  //----------------------------------- IN MATCH ------------------------------------------
  bool isInGame()
  {
    return *(BW::BWDATA_InGame) != 0;
  }
  //----------------------------------- MULTIPLAYER ---------------------------------------
  bool isMultiplayer()
  {
    return (*BW::BWDATA_IsMultiplayer != 0);
  }
  //----------------------------------- SINGLEPLAYER --------------------------------------
  bool isSingleplayer()
  {
    return (*BW::BWDATA_IsMultiplayer == 0);
  }
  //----------------------------------- IS IN LOBBY ---------------------------------------
  bool isInLobby()
  {
    return *BW::BWDATA_NextMenu==3;
  }
  //----------------------------------- IS PAUSED -----------------------------------------
  bool isPaused()
  {
    return *BW::BWDATA_IsNotPaused == 0;
  }
  //----------------------------------- GET MOUSE X ---------------------------------------
  int getMouseX()
  {
    // Retrieves the mouse's X coordinate
    return *(BW::BWDATA_MouseX);
  }
  //----------------------------------- GET MOUSE Y ---------------------------------------
  int getMouseY()
  {
    // Retrieves the mouse's Y coordinate
    return *(BW::BWDATA_MouseY);
  }
  //----------------------------------- GET SCREEN X --------------------------------------
  int getScreenX()
  {
    // Retrieves the screen's X coordinate in relation to the map
    return *(BW::BWDATA_ScreenX);
  }
  //----------------------------------- GET SCREEN Y --------------------------------------
  int getScreenY()
  {
    // Retrieves the screen's Y coordinate in relation to the map
    return *(BW::BWDATA_ScreenY);
  }
  //-------------------------------- SET SCREEN POSITION ----------------------------------
  void setScreenPosition(int x, int y)
  {
    // Sets the screen's position in relation to the map
    *(BW::BWDATA_ScreenX) = x;
    *(BW::BWDATA_ScreenY) = y;
  }
  //------------------------------------ START GAME ---------------------------------------
  void startGame()
  {
    // Starts the game as a lobby host
    issueCommand(BW::Command::StartGame());
  }
  //------------------------------------ PAUSE GAME ---------------------------------------
  void pauseGame()
  {
    // Pauses the game
    issueCommand(BW::Command::PauseGame());
  }
  //------------------------------------ RESUME GAME --------------------------------------
  void resumeGame()
  {
    // Resumes the game
    issueCommand(BW::Command::ResumeGame());
  }
  //------------------------------------- LEAVE GAME --------------------------------------
  void leaveGame()
  {
    // Leaves the current game. Moves directly to the post-game score screen
    *BW::BWDATA_GameState = 0;
    *BW::BWDATA_GamePosition = 6;
  }
  //------------------------------------- RESTART GAME ------------------------------------
  void restartGame()
  {
    // Restarts the current match 
    // Does not work on Battle.net
    *BW::BWDATA_GameState = 0;
    *BW::BWDATA_GamePosition = 5;
  }
  //------------------------------------- GET LATENCY -------------------------------------
  int getLatency()
  {
    // Returns the real latency values

    // special case
    if (BW::isSingleplayer())
      return BW::Latencies::Singleplayer;

    /* Lame options checking */
    switch(*BW::BWDATA_Latency)
    {
      case 0:
        return BW::Latencies::LanLow;
      case 1:
        return BW::Latencies::LanMedium;
      case 2:
        return BW::Latencies::LanHigh;
      default:
        return BW::Latencies::LanLow;
    }
  }
  //------------------------------------ CHANGE SLOT ---------------------------------------
  void changeSlot(BW::SlotID slot, BW::SlotStateID slotState)
  {
    issueCommand(BW::Command::ChangeSlot(slot, slotState));
  }
}