#pragma once
#include "CommandData.h"
#include "GameData.h"
#include "GameImpl.h"
#include "ForceImpl.h"
#include "PlayerImpl.h"
#include "UnitImpl.h"
#include "GameTable.h"

#include <Windows.h>


namespace BWAPI
{
  class Client
  {
  public:
    Client();
    ~Client();

    bool isConnected() const;
    bool connect();
    void disconnect();
    void update();

    /// The state plane, mapped read-only: this process may look at the game and may not write
    /// to it (ADR 0001 section 2, defect 2.3).
    GameData* data = nullptr;

    /// The command plane, mapped read-write: everything this process is allowed to say.
    CommandData* commandData = nullptr;

    /// The client's own copy of the unit and player state, refreshed from the read-only plane
    /// once per frame, and the memory UnitImpl::self and PlayerImpl::self actually point at.
    ///
    /// Latency compensation (CommandTemp.h) predicts the effect of a command locally so that a
    /// bot reading getTarget() straight after attack() sees what it asked for rather than
    /// waiting the two or three frames the server needs. It does that by writing 274 unit fields
    /// and 36 player fields - which it used to write into shared memory, so the plane the server
    /// publishes briefly contained the client's guesses. A prediction is the client's, and now it
    /// lives in the client.
    UnitData unitMirror[GameData::MAX_UNITS] = {};
    PlayerData playerMirror[12] = {};

    /// Refresh the mirror from the state plane. Called once per frame, after the server hands
    /// the frame over and before any event reaches the bot.
    void refreshMirror();
  private:
    HANDLE      pipeObjectHandle;
    HANDLE      mapFileHandle;
    HANDLE      commandMapFileHandle;
    HANDLE      gameTableFileHandle;
    GameTable*  gameTable = nullptr;
    
    bool connected = false;
  };
  extern Client BWAPIClient;
}
