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
    
    /// The handle this bot has for \p unit, issuing one if it does not have it yet.
    ///
    /// Called only where the bot is being told the unit exists: the frame it becomes accessible,
    /// and the loop that publishes the accessible set. Handles are therefore dense in the order
    /// *this bot* discovered units, which is the whole point - see lookupUnitID.
    int       issueUnitID(Unit unit);

    /// The handle this bot already has for \p unit, or -1 if it has never been issued one.
    ///
    /// Everything that mentions a unit in passing goes through here rather than through
    /// issueUnitID: a marine's target, an addon, a carrier, a nydus exit. Upstream allocates at
    /// those sites too, so a bot could be handed a live handle to a unit it has never seen, and
    /// the handle's value told it how many units the game had created before it.
    int       lookupUnitID(Unit unit) const;

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
