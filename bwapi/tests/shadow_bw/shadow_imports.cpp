// The import table BWAPI expects to find in StarCraft.exe.
//
// HackUtil::PatchImport walks the *host process's* import descriptors looking for a named module
// and a named-or-ordinal entry, and overwrites the address the loader put there. It patches what it
// finds; an entry that is not in the table is silently skipped. So a host that imported none of
// these would report that BWAPI redirected nothing - and that report would compare equal between
// two builds while proving nothing at all, which is the one way this test could fail open.
//
// The table below is therefore both the reason the imports exist and the list the host checks
// against after collecting them, so the two cannot drift apart. Every entry is one BWAPI detours in
// CodePatch.cpp; the storm entries are imported by ordinal because storm.def exports them NONAME,
// which is also how StarCraft.exe imports them and how PatchImport looks them up. Keep this in step
// with ApplyCodePatches.

#include <windows.h>

#include "storm.h"

#include "shadow_imports.h"

// Volatile so the optimiser cannot decide the addresses are unused and let the linker drop the
// imports along with them.
static AnyProc volatile sink;

#define DETOUR(module, symbol, fn) { module, symbol, reinterpret_cast<AnyProc>(&fn) }

const DetouredImport detouredImports[] = {
  DETOUR("storm.dll",    "#119",                    SNetLeaveGame),
  DETOUR("storm.dll",    "#121",                    SNetReceiveMessage),
  DETOUR("storm.dll",    "#128",                    SNetSendTurn),
  DETOUR("storm.dll",    "#267",                    SFileOpenFile),
  DETOUR("storm.dll",    "#268",                    SFileOpenFileEx),
  DETOUR("storm.dll",    "#342",                    SDrawCaptureScreen),
  DETOUR("storm.dll",    "#350",                    SDrawLockSurface),
  DETOUR("storm.dll",    "#356",                    SDrawUnlockSurface),
  DETOUR("storm.dll",    "#357",                    SDrawUpdatePalette),
  DETOUR("storm.dll",    "#401",                    SMemAlloc),
  DETOUR("storm.dll",    "#501",                    SStrCopy),

  DETOUR("user32.dll",   "GetCursorPos",            GetCursorPos),
  DETOUR("user32.dll",   "SetCursorPos",            SetCursorPos),
  DETOUR("user32.dll",   "ClipCursor",              ClipCursor),
  DETOUR("user32.dll",   "CreateWindowExA",         CreateWindowExA),

  DETOUR("kernel32.dll", "DeleteFileA",             DeleteFileA),
  DETOUR("kernel32.dll", "GetFileAttributesA",      GetFileAttributesA),
  DETOUR("kernel32.dll", "CreateFileA",             CreateFileA),
  DETOUR("kernel32.dll", "FindFirstFileA",          FindFirstFileA),
  DETOUR("kernel32.dll", "Sleep",                   Sleep),
  DETOUR("kernel32.dll", "CreateThread",            CreateThread),
  DETOUR("kernel32.dll", "CreateEventA",            CreateEventA),
  DETOUR("kernel32.dll", "GetSystemTimeAsFileTime", GetSystemTimeAsFileTime),
  DETOUR("kernel32.dll", "GetCommandLineA",         GetCommandLineA),
};

const unsigned detouredImportCount = sizeof(detouredImports) / sizeof(detouredImports[0]);

#undef DETOUR

void referenceDetouredImports()
{
  for (unsigned i = 0; i < detouredImportCount; ++i)
    sink = detouredImports[i].address;
}
