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
