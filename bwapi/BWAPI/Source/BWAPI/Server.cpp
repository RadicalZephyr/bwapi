#include "Server.h"

#include <cstdio>
#include <ctime>
#include <Util/Convenience.h>
#include <cassert>
#include <sstream>

#include "ClientInput.h"
#include "MonotonicClock.h"
#include "GameImpl.h"
#include "PlayerImpl.h"
#include "UnitImpl.h"
#include "BulletImpl.h"
#include "RegionImpl.h"
#include <BWAPI/Client/CommandData.h>
#include <BWAPI/Client/GameData.h>
#include <BWAPI/Client/GameTable.h>

#include <BW/Pathing.h>
#include <BW/Offsets.h>

#include "../Config.h"
#include <svnrev.h>

#include <Debug.h>

namespace BWAPI
{
  const int PIPE_TIMEOUT = 3000;
  const int PIPE_SYSTEM_BUFFER_SIZE = 4096;

  const BWAPI::GameInstance GameInstance_None(0, false, 0);
  Server::Server()
  {
    // Local variables
    const DWORD processID = GetCurrentProcessId();

    if ( serverEnabled )
    {
      // Try to open the game table
      gameTableFileHandle = CreateFileMappingA( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(GameTable), "Local\\bwapi_shared_memory_game_list" );
      DWORD dwFileMapErr = GetLastError();
      if ( gameTableFileHandle )
      {
        gameTable = static_cast<GameTable*>(MapViewOfFile(gameTableFileHandle, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, sizeof(GameTable)));

        if ( gameTable )
        {
          if ( dwFileMapErr != ERROR_ALREADY_EXISTS )
          {
            // If we created it, initialize it
            for(int i = 0; i < GameTable::MAX_GAME_INSTANCES; ++i)
              gameTable->gameInstances[i] = GameInstance_None;
          } // If does not already exist

          // Check to see if we are already in the table
          for(int i = 0; i < GameTable::MAX_GAME_INSTANCES; ++i)
          {
            if (gameTable->gameInstances[i].serverProcessID == processID)
            {
              gameTableIndex = i;
              break;
            }
          }
          // If not, try to find an empty row
          if (gameTableIndex == -1)
          {
            for(int i = 0; i < GameTable::MAX_GAME_INSTANCES; ++i)
            {
              if (gameTable->gameInstances[i].serverProcessID == 0)
              {
                gameTableIndex = i;
                break;
              }
            }
          }
          // If we can't find an empty row, take over the row with the oldest keep alive time
          if (gameTableIndex == -1)
          {
            DWORD oldest = gameTable->gameInstances[0].lastKeepAliveTime;
            gameTableIndex = 0;
            for(int i = 1; i < GameTable::MAX_GAME_INSTANCES; ++i)
            {
              if (gameTable->gameInstances[i].lastKeepAliveTime < oldest)
              {
                oldest = gameTable->gameInstances[i].lastKeepAliveTime;
                gameTableIndex = i;
              }
            }
          }
          //We have a game table index now, initialize our row
          gameTable->gameInstances[gameTableIndex].serverProcessID = processID;
          gameTable->gameInstances[gameTableIndex].isConnected = false;
          gameTable->gameInstances[gameTableIndex].lastKeepAliveTime = Clock::millis();
        } // if gameTable
      } // if gameTableFileHandle

      // Create the share name
      std::stringstream ssShareName;
      ssShareName << "Local\\bwapi_shared_memory_";
      ssShareName << processID;

      // Two sections, because they have different owners. The state plane is written here and
      // read by the client; the command plane is written by the client and read here. The client
      // maps the first read-only, which is the whole point of the split (defect 2.3).
      std::stringstream ssCommandName;
      ssCommandName << "Local\\bwapi_command_memory_";
      ssCommandName << processID;

      mapFileHandle = CreateFileMappingA( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(GameData), ssShareName.str().c_str() );
      if ( mapFileHandle )
        data = static_cast<GameData*>(MapViewOfFile(mapFileHandle, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, sizeof(GameData)));

      commandMapFileHandle = CreateFileMappingA( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(CommandData), ssCommandName.str().c_str() );
      if ( commandMapFileHandle )
        commandData = static_cast<CommandData*>(MapViewOfFile(commandMapFileHandle, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0, sizeof(CommandData)));
    } // if serverEnabled

    // check if memory was created or if we should create it locally
    if ( !data )
    {
      data = new GameData;
      localOnly = true;
    }
    if ( !commandData )
      commandData = new CommandData();
    initializeSharedMemory();

    if ( serverEnabled )
    {
      // No security descriptor: the default DACL grants the account this process runs as and
      // nothing else.
      //
      // Upstream built an explicit one granting Everyone GENERIC_ALL, which is wider than the
      // default it replaced and is the opposite of what a tournament wants. Narrowing it to the
      // running account is as far as a fork can go: an OS privilege boundary between bot and
      // game needs separate accounts, which ADR 0001 section 2 lists among the things a fork
      // cannot reach.

      std::stringstream communicationPipe;
      communicationPipe << "\\\\.\\pipe\\bwapi_pipe_";
      communicationPipe << processID;
      
      // FILE_FLAG_OVERLAPPED is what makes the wait on the client boundable at all. Without
      // it a ReadFile on a PIPE_WAIT handle blocks until the client answers or the pipe breaks,
      // and a bot that hangs wedges the game forever - defect 2.1 in ADR 0001 section 2.
      pipeObjectHandle = CreateNamedPipeA(communicationPipe.str().c_str(),
                                         PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                         PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                         PIPE_UNLIMITED_INSTANCES,
                                         PIPE_SYSTEM_BUFFER_SIZE,
                                         PIPE_SYSTEM_BUFFER_SIZE,
                                         PIPE_TIMEOUT,
                                         &sa);

      // Manual-reset, initially unsignalled. One event per outstanding operation, and there is
      // never more than one of each: the connect completes before any frame is exchanged.
      connectEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
      ioEvent      = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    }
  }
  Server::~Server()
  {
    if ( pipeObjectHandle && pipeObjectHandle != INVALID_HANDLE_VALUE )
    {
      CancelIoEx(pipeObjectHandle, nullptr);
      DisconnectNamedPipe(pipeObjectHandle);
    }

    if ( connectEvent )
      CloseHandle(connectEvent);
    if ( ioEvent )
      CloseHandle(ioEvent);

    if ( localOnly && data )
    {
      delete data;
      data = nullptr;
    }

    if ( commandMapFileHandle )
      CloseHandle(commandMapFileHandle);
    else
      delete commandData;
    commandData = nullptr;

  }
  void Server::update()
  {
    // Reset data coming in to server
    commandData->stringCount      = 0;
    commandData->commandCount     = 0;
    commandData->unitCommandCount = 0;
    commandData->shapeCount       = 0;
    if (gameTable && gameTableIndex >= 0)
    {
      // A client picks the instance with the oldest keep-alive, and GetTickCount resolves
      // about 16 ms, so two instances launched together used to tie.
      gameTable->gameInstances[gameTableIndex].lastKeepAliveTime = Clock::millis();
      gameTable->gameInstances[gameTableIndex].isConnected = connected;
    }
    if (connected)
    {
      // Update BWAPI Client
      updateSharedMemory();
      meterFrame();
      // A client that never handed the frame back left whatever it had written half-finished;
      // there is nothing there worth applying.
      if (connected)
        processCommands();
    }
    else
    {
      // No client attached: the match runs unattended. Events are produced and dropped, and the
      // connection window stays open only until the first in-game frame - a client that misses
      // the menu does not get a second chance.
      BroodwarImpl.events.clear();
      if (!BroodwarImpl.startedClient)
        checkForConnections();
    }
    // Reset data going out to client
    data->eventCount = 0;
    data->eventStringCount = 0;
  }
  bool Server::isConnected() const
  {
    return connected;
  }
  int Server::addString(const char* text)
  {
    const int slot = ClientInput::reserveSlot(data->eventStringCount, GameData::MAX_EVENT_STRINGS);
    if (slot < 0)
      return -1;
    StrCopy(data->eventStrings[slot], text);
    return slot;
  }
  int Server::addEvent(const BWAPI::Event& e)
  {
    const int slot = ClientInput::reserveSlot(data->eventCount, GameData::MAX_EVENTS);
    if (slot < 0)
      return -1;
    BWAPIC::Event* e2 = &(data->events[slot]);
    int id   = slot + 1;
    e2->type = e.getType();
    e2->v1   = 0;
    e2->v2   = 0;
    switch (e.getType())
    {
    case BWAPI::EventType::MatchEnd:
      e2->v1 = e.isWinner();
      break;
    case BWAPI::EventType::SendText:
    case BWAPI::EventType::SaveGame:
      e2->v1 = addString(e.getText().c_str());
      break;
    case BWAPI::EventType::PlayerLeft:
      e2->v1 = getPlayerID(e.getPlayer());
      break;
    case BWAPI::EventType::ReceiveText:
      e2->v1  = getPlayerID(e.getPlayer());
      e2->v2  = addString(e.getText().c_str());
      break;
    case BWAPI::EventType::NukeDetect:
      e2->v1 = e.getPosition().x;
      e2->v2 = e.getPosition().y;
      break;
    case BWAPI::EventType::UnitDiscover:
    case BWAPI::EventType::UnitEvade:
    case BWAPI::EventType::UnitCreate:
    case BWAPI::EventType::UnitDestroy:
    case BWAPI::EventType::UnitMorph:
    case BWAPI::EventType::UnitShow:
    case BWAPI::EventType::UnitHide:
    case BWAPI::EventType::UnitRenegade:
    case BWAPI::EventType::UnitComplete:
      e2->v1 = issueUnitID(e.getUnit());
      break;
    default:
      break;
    }
    return id;
  }

  // Poll for a client without blocking the menu.
  //
  // The pipe is overlapped now, so ConnectNamedPipe returns immediately with ERROR_IO_PENDING and
  // completes later; the operation is left outstanding between calls and its event is checked
  // with a zero timeout, which is the same "look, do not wait" this always had.
  void Server::checkForConnections()
  {
    if (connected || localOnly || !pipeObjectHandle || pipeObjectHandle == INVALID_HANDLE_VALUE )
      return;

    if (!connectPending)
    {
      ResetEvent(connectEvent);
      connectOverlapped = {};
      connectOverlapped.hEvent = connectEvent;

      if (ConnectNamedPipe(pipeObjectHandle, &connectOverlapped))
      {
        connected = true;   // cannot happen on an overlapped pipe, but the API allows it
        return;
      }
      switch (GetLastError())
      {
      case ERROR_PIPE_CONNECTED:  // a client got there between the create and the connect
        connected = true;
        return;
      case ERROR_IO_PENDING:
        connectPending = true;
        return;
      default:
        return;
      }
    }

    if (WaitForSingleObject(connectEvent, 0) != WAIT_OBJECT_0)
      return;

    DWORD transferred = 0;
    if (GetOverlappedResult(pipeObjectHandle, &connectOverlapped, &transferred, FALSE))
    {
      connectPending = false;
      connected = true;
    }
  }

  // Write on the overlapped handle. Every operation on it must carry an OVERLAPPED, including
  // the ones that would never have blocked.
  bool Server::pipeWrite(const void *buffer, DWORD size)
  {
    ResetEvent(ioEvent);
    OVERLAPPED ov = {};
    ov.hEvent = ioEvent;

    DWORD written = 0;
    if (WriteFile(pipeObjectHandle, buffer, size, &written, &ov))
      return written == size;

    if (GetLastError() != ERROR_IO_PENDING)
      return false;

    if (!GetOverlappedResult(pipeObjectHandle, &ov, &written, TRUE))
      return false;
    return written == size;
  }

  // Read on the overlapped handle, giving up after timeoutMicros. Zero means wait forever, which
  // is what BWAPI has always done and remains the default.
  Server::PipeResult Server::pipeRead(void *buffer, DWORD size, long long timeoutMicros)
  {
    ResetEvent(ioEvent);
    OVERLAPPED ov = {};
    ov.hEvent = ioEvent;

    DWORD received = 0;
    if (ReadFile(pipeObjectHandle, buffer, size, &received, &ov))
      return received == size ? PipeResult::Ok : PipeResult::Failed;

    if (GetLastError() != ERROR_IO_PENDING)
      return PipeResult::Failed;

    const DWORD waitMs = timeoutMicros > 0 ? Clock::waitMillis(timeoutMicros) : INFINITE;

    const DWORD waited = WaitForSingleObject(ioEvent, waitMs);
    if (waited == WAIT_TIMEOUT)
    {
      // Cancel and reap, so no completion lands in this OVERLAPPED after it goes out of scope.
      CancelIoEx(pipeObjectHandle, &ov);
      GetOverlappedResult(pipeObjectHandle, &ov, &received, TRUE);
      return PipeResult::TimedOut;
    }
    if (waited != WAIT_OBJECT_0)
      return PipeResult::Failed;

    if (!GetOverlappedResult(pipeObjectHandle, &ov, &received, FALSE))
      return PipeResult::Failed;
    return received == size ? PipeResult::Ok : PipeResult::Failed;
  }

  // End the match's connection and say why.
  //
  // ADR decision 1: the deadline is a bounded wait, and the adjudication rule belongs to the
  // referee this library does not contain. So expiry does what a broken pipe has always done -
  // stop waiting, record the cause - and the match plays on unattended.
  void Server::disconnectClient(const char *reason, long long elapsedMicros)
  {
    CancelIoEx(pipeObjectHandle, nullptr);
    DisconnectNamedPipe(pipeObjectHandle);
    connected = false;
    connectPending = false;

    BWAPIError("Client disconnected: %s after %lld us on frame %d.",
               reason, elapsedMicros, Broodwar->getFrameCount());
  }
  void Server::initializeSharedMemory()
  {
    //called once when Starcraft starts. Not at the start of every match.
    data->instanceID       = gdwProcNum;
    data->revision         = SVN_REV;
    data->client_version   = CLIENT_VERSION;
    data->isDebug          = (BUILD_DEBUG == 1);
    data->eventCount       = 0;
    data->eventStringCount = 0;
    commandData->commandCount     = 0;
    commandData->unitCommandCount = 0;
    commandData->shapeCount       = 0;
    commandData->stringCount      = 0;
    data->mapFileName[0]   = 0;
    data->mapPathName[0]   = 0;
    data->mapName[0]       = 0;
    data->mapHash[0]       = 0;
    data->hasGUI           = true;
    data->hasLatCom        = true;
    // The localOnly path allocates GameData with new and it has no constructor, so the meter
    // starts at whatever was on the heap unless it is set here.
    commandData->clientWakeMicros        = 0;
    commandData->clientReplyMicros       = 0;
    data->lastFrameDurationMicros = 0;
    data->lastIpcDurationMicros   = 0;
    clearAll();
  }
  void Server::onMatchStart()
  {
    data->self          = getPlayerID(Broodwar->self());
    data->enemy         = getPlayerID(Broodwar->enemy());
    data->neutral       = getPlayerID(Broodwar->neutral());
    data->isMultiplayer = Broodwar->isMultiplayer();
    data->isBattleNet   = Broodwar->isBattleNet();
    data->isReplay      = Broodwar->isReplay();

    // Locally store the map size
    TilePosition mapSize( Broodwar->mapWidth(), Broodwar->mapHeight() );
    WalkPosition mapWalkSize( mapSize );

    // Load walkability
    for ( int x = 0; x < mapWalkSize.x; ++x )
      for ( int y = 0; y < mapWalkSize.y; ++y )
      {
        data->isWalkable[x][y] = Broodwar->isWalkable(x, y);
      }

    // Load buildability, ground height, tile region id
    for ( int x = 0; x < mapSize.x; ++x )
      for ( int y = 0; y < mapSize.y; ++y )
      {
        data->isBuildable[x][y] = Broodwar->isBuildable(x, y);
        data->getGroundHeight[x][y] = Broodwar->getGroundHeight(x, y);
        if (BW::BWDATA::SAIPathing )
          data->mapTileRegionId[x][y] = BW::BWDATA::SAIPathing->mapTileRegionId[y][x];
        else
          data->mapTileRegionId[x][y] = 0;
      }

    // Load pathing info
    if ( BW::BWDATA::SAIPathing )
    {
      data->regionCount = BW::BWDATA::SAIPathing->regionCount;
      for(int i = 0; i < 5000; ++i)
      {
        data->mapSplitTilesMiniTileMask[i] = BW::BWDATA::SAIPathing->splitTiles[i].minitileMask;
        data->mapSplitTilesRegion1[i] = BW::BWDATA::SAIPathing->splitTiles[i].rgn1;
        data->mapSplitTilesRegion2[i] = BW::BWDATA::SAIPathing->splitTiles[i].rgn2;

        BWAPI::Region r = Broodwar->getRegion(i);
        if (r)
        {
          data->regions[i] = *static_cast<RegionImpl*>(r)->getData();
        }
        else
        {
          MemZero(data->regions[i]);
        }
      }
    }

    // Store the map size
    data->mapWidth  = mapSize.x;
    data->mapHeight = mapSize.y;

    // Retrieve map strings
    StrCopy(data->mapFileName, Broodwar->mapFileName());
    StrCopy(data->mapPathName, Broodwar->mapPathName());
    StrCopy(data->mapName, Broodwar->mapName());
    StrCopy(data->mapHash, Broodwar->mapHash());

    data->startLocationCount = Broodwar->getStartLocations().size();
    int idx = 0;
    for (TilePosition t : Broodwar->getStartLocations())
    {
      data->startLocations[idx].x = t.x;
      data->startLocations[idx].y = t.y;
      idx++;
    }

    //static force data
    data->forces[0].name[0] = '\0';
    for(Force i : Broodwar->getForces())
    {
      int id = getForceID(i);
      StrCopy(data->forces[id].name, i->getName());
    }

    //static player data
    for(Player i : Broodwar->getPlayers())
    {
      int id = getPlayerID(i);
      PlayerData* p = &(data->players[id]);
      PlayerData* p2 = static_cast<PlayerImpl*>(i)->self;

      StrCopy(p->name, i->getName());
      p->race = i->getRace();
      p->type = i->getType();
      p->force = getForceID(i->getForce());
      p->color = p2->color;
      p->isParticipating = p2->isParticipating;

      for(int j = 0; j < 12; ++j)
      {
        p->isAlly[j] = false;
        p->isEnemy[j] = false;
      }
      for(Player j : Broodwar->getPlayers())
      {
        p->isAlly[getPlayerID(j)] = i->isAlly(j);
        p->isEnemy[getPlayerID(j)] = i->isEnemy(j);
      }
      p->isNeutral = i->isNeutral();
      p->startLocationX = i->getStartLocation().x;
      p->startLocationY = i->getStartLocation().y;
    }

    data->forceCount = forceVector.size();
    data->playerCount = playerVector.size();
    data->initialUnitCount = unitVector.size();

    data->botAPM_noselects = 0;
    data->botAPM_selects = 0;
  }
  void Server::clearAll()
  {
    //clear force info
    data->forceCount = 0;
    forceVector.clear();
    forceLookup.clear();

    //clear player info
    data->playerCount = 0;
    playerVector.clear();
    playerLookup.clear();

    //clear unit info
    data->initialUnitCount = 0;
    unitVector.clear();
    unitLookup.clear();
  }

  void Server::updateSharedMemory()
  {
    for (Unit u : BroodwarImpl.evadeUnits)
    {
      const int id = lookupUnitID(u);
      if (id >= 0)
        data->units[id] = static_cast<UnitImpl*>(u)->data;
    }

    data->frameCount              = Broodwar->getFrameCount();
    data->replayFrameCount        = Broodwar->getReplayFrameCount();
    data->fps                     = Broodwar->getFPS();
    data->botAPM_noselects        = Broodwar->getAPM(false);
    data->botAPM_selects          = Broodwar->getAPM(true);
    data->latencyFrames           = Broodwar->getLatencyFrames();
    data->latencyTime             = Broodwar->getLatencyTime();
    data->remainingLatencyFrames  = Broodwar->getRemainingLatencyFrames();
    data->remainingLatencyTime    = Broodwar->getRemainingLatencyTime();
    data->elapsedTime             = Broodwar->elapsedTime();
    data->countdownTimer          = Broodwar->countdownTimer();
    data->averageFPS              = Broodwar->getAverageFPS();
    data->mouseX                  = Broodwar->getMousePosition().x;
    data->mouseY                  = Broodwar->getMousePosition().y;
    data->isInGame                = Broodwar->isInGame();
    if (Broodwar->isInGame())
    {
      data->gameType  = Broodwar->getGameType();
      data->latency   = Broodwar->getLatency();
      
      // Copy the mouse states
      for(int i = 0; i < M_MAX; ++i)
        data->mouseState[i]  = Broodwar->getMouseState((MouseButton)i);
      
      // Copy the key states
      for(int i = 0; i < K_MAX; ++i)
        data->keyState[i]  = Broodwar->getKeyState((Key)i);

      // Copy the screen position
      data->screenX  = Broodwar->getScreenPosition().x;
      data->screenY  = Broodwar->getScreenPosition().y;

      for ( int i = 0; i < BWAPI::Flag::Max; ++i )
        data->flags[i] = Broodwar->isFlagEnabled(i);

      data->isPaused = Broodwar->isPaused();
      data->selectedUnitCount = Broodwar->getSelectedUnits().size();

      int idx = 0;
      for(Unit t : Broodwar->getSelectedUnits())
        data->selectedUnits[idx++] = lookupUnitID(t);

      //dynamic map data
      Map::copyToSharedMemory();
      //(no dynamic force data)

      //dynamic player data
      for(Player i : Broodwar->getPlayers())
      {
        int id         = getPlayerID(i);
        if ( id >= 12 )
          continue;
        PlayerData* p  = &(data->players[id]);
        PlayerData* p2 = static_cast<PlayerImpl*>(i)->self;

        p->isVictorious     = i->isVictorious();
        p->isDefeated       = i->isDefeated();
        p->leftGame         = i->leftGame();
        p->minerals         = p2->minerals;
        p->gas              = p2->gas;
        p->gatheredMinerals = p2->gatheredMinerals;
        p->gatheredGas      = p2->gatheredGas;
        p->repairedMinerals = p2->repairedMinerals;
        p->repairedGas      = p2->repairedGas;
        p->refundedMinerals = p2->refundedMinerals;
        p->refundedGas      = p2->refundedGas;
        for(int j = 0; j < 3; ++j)
        {
          p->supplyTotal[j]  = p2->supplyTotal[j];
          p->supplyUsed[j]  = p2->supplyUsed[j];
        }
        for(int j = 0; j < UnitTypes::Enum::MAX; ++j)
        {
          p->allUnitCount[j]        = p2->allUnitCount[j];
          p->visibleUnitCount[j]    = p2->visibleUnitCount[j];
          p->completedUnitCount[j]  = p2->completedUnitCount[j];
          p->deadUnitCount[j]       = p2->deadUnitCount[j];
          p->killedUnitCount[j]     = p2->killedUnitCount[j];
        }
        p->totalUnitScore     = p2->totalUnitScore;
        p->totalKillScore     = p2->totalKillScore;
        p->totalBuildingScore = p2->totalBuildingScore;
        p->totalRazingScore   = p2->totalRazingScore;
        p->customScore        = p2->customScore;

        for(int j = 0; j < 63; ++j)
        {
          p->upgradeLevel[j] = p2->upgradeLevel[j];
          p->isUpgrading[j]  = p2->isUpgrading[j];
        }

        for(int j = 0; j < 47; ++j)
        {
          p->hasResearched[j] = p2->hasResearched[j];
          p->isResearching[j] = p2->isResearching[j];
        }
        memcpy(p->isResearchAvailable, p2->isResearchAvailable, sizeof(p->isResearchAvailable));
        memcpy(p->isUnitAvailable, p2->isUnitAvailable, sizeof(p->isUnitAvailable));
        memcpy(p->maxUpgradeLevel, p2->maxUpgradeLevel, sizeof(p->maxUpgradeLevel));
      }

      //dynamic unit data
      for(Unit i : Broodwar->getAllUnits())
      {
        const int id = issueUnitID(i);
        if (id >= 0)
          data->units[id] = static_cast<UnitImpl*>(i)->data;
      }

      for(int i = 0; i < BW::UNIT_ARRAY_MAX_LENGTH; ++i)
      {
        Unit u = Broodwar->indexToUnit(i);
        int id = -1;
        if ( u )
          id = lookupUnitID(u);
        data->unitArray[i] = id;
      }

      unitFinder* xf = data->xUnitSearch;
      unitFinder* yf = data->yUnitSearch;
      const BW::unitFinder* bwxf = BW::BWDATA::UnitOrderingX.data();
      const BW::unitFinder* bwyf = BW::BWDATA::UnitOrderingY.data();
      int bwSearchSize = BW::BWDATA::UnitOrderingCount;

      for ( int i = 0; i < bwSearchSize; ++i, bwxf++, bwyf++ )
      {
        if (bwxf->unitIndex > 0 && bwxf->unitIndex <= BW::UNIT_ARRAY_MAX_LENGTH)
        {
          UnitImpl* u = BroodwarImpl.unitArray[bwxf->unitIndex-1];
          if ( u && u->canAccess() )
          {
            xf->searchValue = bwxf->searchValue;
            xf->unitIndex = lookupUnitID(u);
            xf++;
          }
        } // x index

        if (bwyf->unitIndex > 0 && bwyf->unitIndex <= BW::UNIT_ARRAY_MAX_LENGTH)
        {
          UnitImpl* u = BroodwarImpl.unitArray[bwyf->unitIndex-1];
          if ( u && u->canAccess() )
          {
            yf->searchValue = bwyf->searchValue;
            yf->unitIndex = lookupUnitID(u);
            yf++;
          }
        } // x index

      } // loop unit finder
      
      // Set size
      data->unitSearchSize = xf - data->xUnitSearch; // we assume an equal number of y values was put into the array
      

      //dynamic bullet data
      for(int id = 0; id < 100; ++id)
        data->bullets[id] = BroodwarImpl.getBulletFromIndex(id)->data;
      
      //dynamic nuke dot data
      int j = 0;
      data->nukeDotCount = Broodwar->getNukeDots().size();
      for(Position const &nd : Broodwar->getNukeDots())
      {
        data->nukeDots[j].x = nd.x;
        data->nukeDots[j].y = nd.y;
        ++j;
      }
    }

    // iterate events
    for (Event &e : BroodwarImpl.events)
    {
      if (e.getType() == EventType::MatchStart)
      {
        onMatchStart();
      }

      // Add the event to the server queue
      addEvent(e);
    }
    BroodwarImpl.events.clear();
  }

  int Server::getForceID(Force force)
  {
    if ( !force )
      return -1;
    if (forceLookup.find(force) == forceLookup.end())
    {
      forceLookup[force] = (int)(forceVector.size());
      forceVector.push_back(force);
    }
    return forceLookup[force];
  }
  Force Server::getForce(int id) const
  {
    if (forceVector.size() <= static_cast<unsigned>(id))
      return nullptr;
    return forceVector[id];
  }
  int Server::getPlayerID(Player player)
  {
    if ( !player )
      return -1;
    if (playerLookup.find(player) == playerLookup.end())
    {
      playerLookup[player] = (int)(playerVector.size());
      playerVector.push_back(player);
    }
    return playerLookup[player];
  }
  Player Server::getPlayer(int id) const
  {
    if (playerVector.size() <= static_cast<unsigned>(id))
      return nullptr;
    return playerVector[id];
  }

  // Defect 2.7 in ADR 0001 section 2, and the reason this is two functions rather than one.
  //
  // Upstream has a single allocate-on-lookup getUnitID, first called from extractUnitData over
  // *every unit alive in the game*. So the handle a bot receives for a scouted enemy marine is
  // that unit's global creation ordinal, and the gap between two of the bot's own consecutive
  // handles is the number of units everyone else created in between - which is how you recognise
  // a four-pool without scouting. The nine call sites in UnitUpdate.cpp make it worse: a visible
  // enemy unit's target field allocated a handle for, and handed the bot, a unit it had never
  // seen.
  //
  // Splitting the function splits the question. Handles are issued where the bot is told a unit
  // exists, and looked up everywhere else, so they are dense in the order this bot discovered
  // things and a unit it has not seen has no handle to leak.
  int Server::issueUnitID(Unit unit)
  {
    if ( !unit )
      return -1;
    auto it = unitLookup.find(unit);
    if (it != unitLookup.end())
      return it->second;

    // The handle is the subscript into data->units, so there is no handle to give past the end
    // of it. Upstream keeps counting, and the writes then run off the array. Whether a match can
    // reach ten thousand handles was never measured, which is exactly the reason not to leave it
    // to chance - and issuing only on exposure makes it far harder to reach in the first place.
    const int id = static_cast<int>(unitVector.size());
    if (id >= GameData::MAX_UNITS)
      return -1;

    unitLookup[unit] = id;
    unitVector.push_back(unit);
    return id;
  }
  int Server::lookupUnitID(Unit unit) const
  {
    if ( !unit )
      return -1;
    auto it = unitLookup.find(unit);
    return it == unitLookup.end() ? -1 : it->second;
  }
  Unit Server::getUnit(int id) const
  {
    if (unitVector.size() <= static_cast<unsigned>(id))
      return nullptr;
    return unitVector[id];
  }

  // Hand the frame to the client, wait for it, and charge what it cost.
  //
  // The server can only see the span from "frame published" to "reply received", and that span
  // contains two pipe round trips the server itself caused. The client stamps the two instants
  // that bracket its own work into the plane, so the bot is charged for its own work and the
  // transport is recorded separately rather than billed to whoever is holding it (ADR 0001
  // section 2, defect 2.5: "a second timestamp so a bot is not billed for the referee's IPC").
  //
  // Only client-to-client and server-to-server differences are taken, so the two clocks never
  // have to agree on an epoch.
  void Server::meterFrame()
  {
    const long long previousReply = commandData->clientReplyMicros;

    const long long handoff = Clock::micros();
    callOnFrame();
    const long long roundTrip = Clock::micros() - handoff;

    long long botSpan = roundTrip;
    if (commandData->clientReplyMicros > commandData->clientWakeMicros &&
        commandData->clientReplyMicros != previousReply)
    {
      // The client stamped a complete frame this time round.
      botSpan = commandData->clientReplyMicros - commandData->clientWakeMicros;
      if (botSpan > roundTrip)
        botSpan = roundTrip;  // the two clocks disagreed; never charge more than really elapsed
    }

    data->lastFrameDurationMicros = botSpan;
    data->lastIpcDurationMicros   = roundTrip - botSpan;
    BroodwarImpl.setLastFrameDurationMicros(botSpan);
  }

  // Publish the frame and wait for the client to hand it back.
  //
  // The wait is bounded by [game] frame_timeout_ms, which defaults to zero - wait forever, which
  // is what this has always done. Above zero, a client that does not answer is disconnected and
  // the match plays on rather than the game hanging on it (ADR 0001 section 2, defect 2.1).
  void Server::callOnFrame()
  {
    const long long timeoutMicros = static_cast<long long>(frameTimeoutMs) * 1000;
    const long long started = Clock::micros();

    int code = 2;
    if (!pipeWrite(&code, sizeof(code)))
    {
      disconnectClient("the pipe broke while publishing the frame", Clock::micros() - started);
      return;
    }

    while (code != 1)
    {
      // The deadline is on the whole exchange, not on each read, or a client that answers with
      // something other than 1 could reset the clock as often as it liked.
      long long remaining = 0;
      if (timeoutMicros > 0)
      {
        remaining = timeoutMicros - (Clock::micros() - started);
        if (remaining <= 0)
        {
          disconnectClient("it did not finish the frame within frame_timeout_ms",
                           Clock::micros() - started);
          return;
        }
      }

      switch (pipeRead(&code, sizeof(code), remaining))
      {
      case PipeResult::Ok:
        break;
      case PipeResult::TimedOut:
        disconnectClient("it did not finish the frame within frame_timeout_ms",
                         Clock::micros() - started);
        return;
      case PipeResult::Failed:
        disconnectClient("the pipe broke while waiting for the frame",
                         Clock::micros() - started);
        return;
      }
    }
  }
  void Server::processCommands()
  {
    // Every count and index below is written by the untrusted client, so each is clamped or
    // range-checked here rather than trusted. The only bound the protocol ships is an assert in
    // the client itself (BWAPIClient/Source/GameImpl.cpp), which NDEBUG compiles out.
    const int stringCount  = ClientInput::clampCount(commandData->stringCount, CommandData::MAX_STRINGS);
    const int commandCount = ClientInput::clampCount(commandData->commandCount, CommandData::MAX_COMMANDS);

    // A string a command names, NUL-terminated, or the empty string if the index is out of range.
    const auto clientString = [&](int index) -> const char * {
      if (!ClientInput::indexInRange(index, stringCount))
        return "";
      return ClientInput::terminate(commandData->strings[index], sizeof(commandData->strings[index]));
    };

    for(int i = 0; i < commandCount; ++i)
    {
      BWAPIC::CommandType::Enum c = commandData->commands[i].type;
      int v1 = commandData->commands[i].value1;
      int v2 = commandData->commands[i].value2;
      switch (c)
      {
      case BWAPIC::CommandType::SetScreenPosition:
        if (Broodwar->isInGame())
          Broodwar->setScreenPosition(v1,v2);
        break;
      case BWAPIC::CommandType::PingMinimap:
        if (Broodwar->isInGame())
          Broodwar->pingMinimap(v1,v2);
        break;
      case BWAPIC::CommandType::EnableFlag:
        if (Broodwar->isInGame())
          Broodwar->enableFlag(v1);
        break;
      case BWAPIC::CommandType::Printf:
        if (Broodwar->isInGame())
          Broodwar->printf("%s", clientString(v1));
        break;
      case BWAPIC::CommandType::SendText:
        if (Broodwar->isInGame())
          Broodwar->sendTextEx(v2 != 0, "%s", clientString(v1));
        break;
      case BWAPIC::CommandType::PauseGame:
        if (Broodwar->isInGame())
          Broodwar->pauseGame();
        break;
      case BWAPIC::CommandType::ResumeGame:
        if (Broodwar->isInGame())
          Broodwar->resumeGame();
        break;
      case BWAPIC::CommandType::LeaveGame:
        if (Broodwar->isInGame())
          Broodwar->leaveGame();
        break;
      case BWAPIC::CommandType::RestartGame:
        if (Broodwar->isInGame())
          Broodwar->restartGame();
        break;
      case BWAPIC::CommandType::SetLocalSpeed:
        if (Broodwar->isInGame())
          Broodwar->setLocalSpeed(v1);
        break;
      case BWAPIC::CommandType::SetLatCom:
        Broodwar->setLatCom(v1 == 1);
        break;
      case BWAPIC::CommandType::SetGui:
        Broodwar->setGUI(v1 == 1);
        break;
      case BWAPIC::CommandType::SetFrameSkip:
        if (Broodwar->isInGame())
          Broodwar->setFrameSkip(v1);
        break;
      case BWAPIC::CommandType::SetMap:
        Broodwar->setMap(clientString(v1));
        break;
      case BWAPIC::CommandType::SetAllies:
        if (Broodwar->isInGame())
          Broodwar->setAlliance(getPlayer(v1), v2 != 0, v2 == 2);
        break;
      case BWAPIC::CommandType::SetVision:
        if (Broodwar->isInGame())
          Broodwar->setVision(getPlayer(v1), v2 != 0);
        break;
      case BWAPIC::CommandType::SetCommandOptimizerLevel:
        if (Broodwar->isInGame())
          Broodwar->setCommandOptimizationLevel(v1);
        break;
      case BWAPIC::CommandType::SetRevealAll:
        if ( Broodwar->isInGame() )
          Broodwar->setRevealAll(v1 != 0);
        break;
      default:
        break;
      }
    }
    if ( Broodwar->isInGame() )
    {
      const int unitCount = static_cast<int>(unitVector.size());
      const int unitCommandCount =
        ClientInput::clampCount(commandData->unitCommandCount, CommandData::MAX_UNIT_COMMANDS);
      for ( int i = 0; i < unitCommandCount; ++i )
      {
        if (!ClientInput::indexInRange(commandData->unitCommands[i].unitIndex, unitCount))
          continue;

        // The type is an enum id the client wrote, and it is not checked downstream: the switch
        // in Templates::canIssueCommandType falls through to `return true` for anything it does
        // not recognise, so an unknown type reaches executeCommand, queues a select order for
        // the unit and charges APM before doing nothing. Twenty thousand of those fit in one
        // frame.
        if (!ClientInput::indexInRange(commandData->unitCommands[i].type, UnitCommandTypes::Enum::MAX))
          continue;

        Unit unit = unitVector[commandData->unitCommands[i].unitIndex];
        Unit target = nullptr;
        if (ClientInput::indexInRange(commandData->unitCommands[i].targetIndex, unitCount))
          target = unitVector[commandData->unitCommands[i].targetIndex];

        unit->issueCommand(UnitCommand(unit, commandData->unitCommands[i].type, target, commandData->unitCommands[i].x, commandData->unitCommands[i].y, commandData->unitCommands[i].extra));
      }
    } // if isInGame
  }

}