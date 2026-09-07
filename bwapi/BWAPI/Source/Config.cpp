#include <limits>
#include <random>
#include <string>
#include <windows.h>
#include <tlhelp32.h>
#include <storm.h>

#include <Util/StringUtil.h>

#include "Config.h"
#include "BWAPI/Permissions.h"

#include "WMode.h"

std::string screenshotFmt;

bool isCorrectVersion = true;
bool showWarn         = true;
bool serverEnabled    = true;
int frameTimeoutMs    = 0;

unsigned gdwProcNum = 1;

//--------------------------------------------- MATCH SEED ---------------------------------------------------
unsigned matchSeed()
{
  static const unsigned seed = [] () -> unsigned {
    const int override_ = LoadConfigInt("starcraft", "seed_override",
                                        std::numeric_limits<int>::max());
    if ( override_ != std::numeric_limits<int>::max() )
      return static_cast<unsigned>(override_);

    // No override: draw once, so the value can still be recorded. GetTickCount could not be -
    // it is not a value anyone chose, it is barely a value at all at 16 ms of resolution, and
    // two instances launched together got the same one.
    std::random_device rd;
    return rd();
  }();
  return seed;
}

//--------------------------------------------- PERMISSIONS --------------------------------------------------
// The [permissions] section of bwapi.ini, replacing TournamentModule::onAction (ADR 0001 section
// 2, defect 2.4). One key per Tournament::ActionID, spelled as the action, plus the one integer
// threshold the reference policy needs. Read once: policy that could be re-read mid-game is
// policy a bot with a filesystem could rewrite.
namespace
{
  // Indexed by Tournament::ActionID, so the order is the enum's.
  const char * const permissionKeys[BWAPI::TOURNAMENT_ACTION_COUNT] = {
    "enable_flag",
    "pause_game",
    "resume_game",
    "leave_game",
    "set_local_speed",
    "set_text_size",
    "set_lat_com",
    "set_gui",
    "set_map",
    "set_frame_skip",
    "printf",
    "send_text",
    "set_command_optimization_level",
  };

  BWAPI::PermissionTable loadPermissions()
  {
    BWAPI::PermissionTable table = BWAPI::PermissionTable::defaults();
    for (int i = 0; i < BWAPI::TOURNAMENT_ACTION_COUNT; ++i)
    {
      // Anything other than ON or OFF leaves the default in place; a typo must not silently
      // widen what a bot may do.
      const std::string value = LoadConfigStringUCase("permissions", permissionKeys[i],
                                                      table.allowed[i] ? "ON" : "OFF");
      if (value == "ON")
        table.allowed[i] = true;
      else if (value == "OFF")
        table.allowed[i] = false;
    }
    table.minCommandOptimization =
      LoadConfigInt("permissions", "min_command_optimization", table.minCommandOptimization);
    return table;
  }
}

const BWAPI::PermissionTable &BWAPI::permissions()
{
  static const BWAPI::PermissionTable table = loadPermissions();
  return table;
}

//--------------------------------------------- GET PROC COUNT -----------------------------------------------
// Found/modified this from some random help board
DWORD getProcessCount(const char *pszProcName)
{
  PROCESSENTRY32 pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32);

  DWORD dwCount = 0;
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if ( Process32First(hSnapshot, &pe32) )
  {
    do
    {
      if( _strcmpi(pe32.szExeFile, pszProcName) == 0 )
        ++dwCount;
    } while( Process32Next(hSnapshot, &pe32) );
  }
  CloseHandle(hSnapshot);
  return dwCount;
}

//----------------------------- LOAD CONFIG FXNS ------------------------------------------
std::string envKeyName(const char *pszKey, const char *pszItem)
{
  return "BWAPI_CONFIG_" + Util::to_upper_copy(pszKey) + "__" + Util::to_upper_copy(pszItem);
}
std::string LoadConfigStringFromFile(const char *pszKey, const char *pszItem, const char *pszDefault)
{
  char buffer[MAX_PATH];
  GetPrivateProfileStringA(pszKey, pszItem, pszDefault ? pszDefault : "", buffer, MAX_PATH, configPath().c_str());
  return std::string(buffer);
}
std::string LoadConfigString(const char *pszKey, const char *pszItem, const char *pszDefault)
{
  std::string envKey = envKeyName(pszKey, pszItem);
  if (char* v = std::getenv(envKey.c_str()))
    return v;
  else
    return LoadConfigStringFromFile(pszKey, pszItem, pszDefault);
}
// this version uppercase result string after loading, should be used for the most of enum-like strings
std::string LoadConfigStringUCase (const char *pszKey, const char *pszItem, const char *pszDefault)
{
  return Util::to_upper_copy(LoadConfigString(pszKey, pszItem, pszDefault));
}
int LoadConfigInt(const char *pszKey, const char *pszItem, const int iDefault)
{
  std::string envKey = envKeyName(pszKey, pszItem);
  if (char* v = std::getenv(envKey.c_str()))
    return std::stoi(v);
  else
    return GetPrivateProfileIntA(pszKey, pszItem, iDefault, configPath().c_str());
}
void WriteConfig(const char *pszKey, const char *pszItem, const std::string& value)
{
  // avoid writing unless the value is actually different, because writing causes
  // an annoying popup when having the file open in e.g. notepad++
  if (LoadConfigStringFromFile(pszKey, pszItem, "_NULL") != value)
    WritePrivateProfileStringA(pszKey, pszItem, value.c_str(), configPath().c_str());
}
void WriteConfig(const char *pszKey, const char *pszItem, int value)
{
  WriteConfig(pszKey, pszItem, std::to_string(value));
}

void InitPrimaryConfig()
{
  static bool isPrimaryConfigInitialized = false;
  // Return if already initialized
  if ( isPrimaryConfigInitialized )
    return;
  isPrimaryConfigInitialized = true;

  // ------------------------- GENERAL/GLOBAL CONFIG OPTIONS ----------------------------------
  // Get screenshot format
  screenshotFmt = LoadConfigString("starcraft", "screenshots", "gif");
  if ( !screenshotFmt.empty() )
    screenshotFmt.insert(0, ".");

  // Check if warning dialogs should be shown
  showWarn = LoadConfigStringUCase("config", "show_warnings", "YES") == "YES";

  // Check if shared memory should be enabled
  serverEnabled = LoadConfigStringUCase("config", "shared_memory", "ON") == "ON";

  // How long to wait for a client to finish a frame. Negative is meaningless; treat it as off.
  frameTimeoutMs = LoadConfigInt("game", "frame_timeout_ms", 0);
  if ( frameTimeoutMs < 0 )
    frameTimeoutMs = 0;

  // Get process count
  gdwProcNum = getProcessCount("StarCraft.exe");

  // ------------------------- WMODE CONFIG OPTIONS ----------------------------------
  // Load windowed mode position and fullscreen setting
  windowRect.left   = LoadConfigInt("window", "left");
  windowRect.top    = LoadConfigInt("window", "top");
  windowRect.right  = LoadConfigInt("window", "width");
  windowRect.bottom = LoadConfigInt("window", "height");
  switchToWMode     = LoadConfigStringUCase("window", "windowed", "OFF") == "ON";

  // Limit minimum w-mode size
  if ( windowRect.right < WMODE_MIN_WIDTH )
    windowRect.right = WMODE_MIN_WIDTH;
  if ( windowRect.bottom < WMODE_MIN_HEIGHT )
    windowRect.bottom = WMODE_MIN_HEIGHT;

}

