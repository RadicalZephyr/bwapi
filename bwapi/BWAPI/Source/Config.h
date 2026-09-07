#pragma once
#include <string>
#include <storm.h>
#include "DLLMain.h"
#include <Util/Path.h>

// Functions
std::string LoadConfigString(const char *pszKey, const char *pszItem, const char *pszDefault = NULL);
std::string LoadConfigStringUCase (const char *pszKey, const char *pszItem, const char *pszDefault = NULL);
int LoadConfigInt(const char *pszKey, const char *pszItem, const int iDefault = 0);
void WriteConfig(const char *pszKey, const char *pszItem, const std::string& value);
void WriteConfig(const char *pszKey, const char *pszItem, int value);

void InitPrimaryConfig();

// The starcraft instance's root directory
inline const std::string& installPath()
{
  static std::string path;
  if (path.empty())
  {
    char buffer[MAX_PATH];
    if (GetModuleFileNameA(NULL, buffer, MAX_PATH)) //get .exe path
      path = Util::Path(buffer).parent_path().string() + "\\";
    else
      BWAPIError("Error getting starcraft's root directory via GetModuleFileNameA()");
  }
  return path;
}
// The config directory
inline const std::string& configDir()
{
  static const std::string path = installPath() + "bwapi-data\\";
  return path;
}
// The bwapi.ini file in the config directory. Can be overridden by environment variable BWAPI_CONFIG_INI
inline const std::string& configPath()
{
  static std::string path;
  if (path.empty())
  {
    char* env = std::getenv("BWAPI_CONFIG_INI");
    path = env ? env : configDir() + "bwapi.ini";
  }
  return path;
}
extern std::string screenshotFmt;

extern bool isCorrectVersion;
extern bool showWarn;
extern bool serverEnabled;

/// How long the server waits for the client to finish a frame, in milliseconds.
///
/// 0 means wait forever, which is what BWAPI has always done and stays the default: this knob
/// changes an operator's behaviour, not everybody's. Above zero, a client that does not answer
/// within the deadline is disconnected and the match plays on (ADR 0001 section 2, defect 2.1).
extern int frameTimeoutMs;

/// The one value every generator BWAPI owns is seeded from.
///
/// Defect 2.8 in ADR 0001 section 2: srand was seeded from GetTickCount, so nothing BWAPI chose
/// at random could be reproduced or even reported. This is [starcraft] seed_override when it is
/// set, and otherwise one draw from the system's entropy source, made once and remembered - so
/// the value exists to be recorded either way, which is the point.
unsigned matchSeed();
extern unsigned gdwProcNum;
