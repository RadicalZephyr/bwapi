#pragma once
#include <windows.h>

#include <vector>
#include <unordered_map>

namespace BWAPI
{
  // Forwards
  struct GameData;
  struct GameTable;
  class Event;
  class ForceInterface;
  typedef ForceInterface* Force;
  class PlayerInterface;
  typedef PlayerInterface* Player;
  class UnitInterface;
  typedef UnitInterface* Unit;

  class Server
  {
  public:
    Server();
    ~Server();

    Server(const Server &other) = delete;
    Server(Server &&other) = delete;
    
    void      update();
    bool      isConnected() const;
    int       addEvent(const BWAPI::Event& e);
    int       addString(const char* text);
    void      clearAll();

    int       getForceID(Force force);
    Force     getForce(int id) const;
    
    int       getPlayerID(Player player);
    Player    getPlayer(int id) const;
    
    int       getUnitID(Unit unit);
    Unit      getUnit(int id) const;

    GameData  *data = nullptr;
  private:
    void onMatchStart();
    void checkForConnections();
    void initializeSharedMemory();
    void updateSharedMemory();
    void meterFrame();
    void callOnFrame();
    void processCommands();
    void disconnectClient(const char *reason, long long elapsedMicros);

    /// The outcome of one bounded pipe operation.
    enum class PipeResult { Ok, TimedOut, Failed };

    /// Blocking write on the overlapped pipe. Four bytes into a 4 KB buffer never really waits.
    bool pipeWrite(const void *buffer, DWORD size);

    /// Read \p size bytes, giving up after \p timeoutMicros. Zero means wait forever.
    PipeResult pipeRead(void *buffer, DWORD size, long long timeoutMicros);

    HANDLE pipeObjectHandle = nullptr;
    HANDLE connectEvent = nullptr;
    HANDLE ioEvent = nullptr;
    OVERLAPPED connectOverlapped = {};
    bool connectPending = false;
    HANDLE mapFileHandle = nullptr;
    HANDLE gameTableFileHandle = nullptr;
    GameTable* gameTable = nullptr;
    int gameTableIndex = -1;
    bool connected = false;
    bool localOnly = false;

    std::vector<Force> forceVector;
    std::unordered_map<Force, int> forceLookup;

    std::vector<Player> playerVector;
    std::unordered_map<Player, int> playerLookup;

    std::vector<Unit> unitVector;
    std::unordered_map<Unit, int> unitLookup;

    PSID pEveryoneSID = nullptr;
    PACL pACL = nullptr;
    PSECURITY_DESCRIPTOR pSD = nullptr;
  };
}
