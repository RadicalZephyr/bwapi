# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Platform reality check

BWAPI is a **Windows-only, 32-bit (Win32), MSVC-only** codebase. It injects a DLL into
`StarCraft.exe` 1.16.1 and reads/writes that process's memory at hardcoded addresses. Practical
consequences when working in a Linux container:

- **Nothing here builds or runs.** No msbuild, no `vstest.console`, no StarCraft. Changes must be
  reasoned about statically, or handed to the user to compile on Windows.
- Sources include `<windows.h>`, `<ddraw.h>`, `<winsock.h>` and use `__stdcall`/`__fastcall`,
  `__declspec(dllexport)`, and structure packing. Don't "portably" rewrite these.
- Pointer-size and struct-layout assumptions are load-bearing — `GameData` is a shared-memory ABI
  and `BW::` structs mirror Broodwar's own memory layout.

## Build and test

Everything is driven by `bwapi/bwapi.sln` (Visual Studio 2017, platform toolset `v141_xp`).

```bat
:: Full release pipeline (build.bat). Needs msbuild, git, doxygen+dot, bash, pngcrush,
:: java, Inno Setup, 7-Zip on PATH.
build.bat

:: Just the code
msbuild /p:Configuration=Debug_Pipeline   bwapi/bwapi.sln
msbuild /p:Configuration=Release_Pipeline bwapi/bwapi.sln
msbuild /p:Configuration=Installer_Target bwapi/bwapi.sln   :: docs + installer + archive

:: Unit tests (MSTest / CppUnitTest, run from bwapi/Debug)
vstest.console BWAPILIBTest.dll BWAPICoreTest.dll
vstest.console BWAPILIBTest.dll /Tests:PositionCtorDefault   :: single test method
vstest.console BWAPILIBTest.dll /Tests:positionTest          :: single TEST_CLASS

:: Static analysis
cppcheck_script.bat   :: writes cppcheck.xml
```

### Configurations

| Configuration | Post-build behaviour |
|---|---|
| `Debug` / `Release` | Copy binaries to `Release_Binary/` **and** into the local StarCraft install (`copyToTarget.vbs` reads the install path from the registry). Use these for local iteration. |
| `Debug_Pipeline` / `Release_Pipeline` | Copy to `Release_Binary/` only. Used by CI. |
| `Debug_NoCopy` / `Release_NoCopy` | Copy nothing. |
| `Installer_Target` | Builds `Documentation` (doxygen) and `Installer` (Inno Setup) on top of the release. |

Only unit tests (`BWAPILIBTest`, `BWAPICoreTest`) are automatable. `TestAIModule` is an
*in-game* test suite: a bot DLL that plays scripted scenarios on the maps in
`bwapi/TestAIModule/maps/` and asserts via `BWAssert.h`. It requires a running StarCraft.

## Architecture

### One interface, two implementations

The public API in `bwapi/include/BWAPI/` is a set of abstract interfaces
(`Game.h`, `UnitInterface` in `Unit.h`, `PlayerInterface`, …). There are **two independent
implementations** of them, and this shapes almost everything else:

1. **Module (server) side** — `bwapi/BWAPI/`, built as `BWAPI.dll`, injected into StarCraft.
   `BWAPI::UnitImpl` here (`BWAPI/Source/BWAPI/UnitImpl.h`) wraps a `BW::CUnit*` pointing into
   Broodwar's live memory.
2. **Client side** — `bwapi/BWAPIClient/` plus headers in `bwapi/include/BWAPI/Client/`, built as
   a static lib for out-of-process bots. `BWAPI::UnitImpl` here
   (`include/BWAPI/Client/UnitImpl.h`) wraps a `UnitData` struct copied out of shared memory.

Both classes have the **same fully-qualified name**. `bwapi/Shared/*.cpp` (`UnitShared.cpp`,
`GameShared.cpp`, `PlayerShared.cpp`, …) is compiled into *both* targets; it says
`#include "UnitImpl.h"` and the include path decides which one it gets. So a change to
`Shared/UnitShared.cpp` must compile and behave correctly against both `UnitImpl` definitions,
and any member it touches must exist in both. Include paths that make this work:

- `BWAPI.vcxproj`: `../Shared;Source/BWAPI;../Util/Source;Source;../include;../Storm;../BWAPICore`
- `BWAPIClient.vcxproj`: `../include;../Shared;../include/BWAPI/Client`

`bwapi/Shared/Templates.h` holds the shared game-rules logic (the huge `canX()` family,
`hasPower`, build-position checks) as free functions in `BWAPI::Templates`, so both `UnitImpl`s
delegate to the same rules.

### Server ↔ client protocol

`BWAPI/Source/BWAPI/Server.cpp` and `BWAPIClient/Source/Client.cpp` are two ends of one channel:

- A named pipe for frame synchronisation, plus a shared-memory mapping holding a single
  `BWAPI::GameData` (`include/BWAPI/Client/GameData.h`) with fixed-size arrays
  (`units[10000]`, `players[12]`, …).
- Each frame the server serialises game state into `GameData` and signals the client; the client
  appends `BWAPIC::Command` records and unit commands into the same struct, and the server
  replays them (`GameCommands.cpp`).
- `GameTable` lets a client discover multiple running StarCraft instances.

`GameData` is an ABI between separately-compiled binaries. Reordering or resizing its fields
breaks every existing client — `client_version` and `revision` are deliberately pinned at the top
of the struct for this reason.

### The `BW::` layer (module side only)

`bwapi/BWAPI/Source/BW/` is the reverse-engineered view of StarCraft:

- `BW/Offsets.h` — all hardcoded addresses, via the `IS_REF(name, addr)` macro that binds a typed
  reference to an absolute address inside the `BW::BWDATA` namespace. **All new offsets go here**
  (hex, per CONTRIBUTING).
- `BW/Structures.h`, `CUnit.h`, `CSprite.h`, `COrder.h`, … — `#pragma pack(1)` mirrors of
  Broodwar's structs.
- `Detours.cpp` / `CodePatch.cpp` — function hooks and in-place code patches; `DLLMain.cpp` is the
  injection entry point and drives the main loop hook (`_nextFrameHook`).
- `WMode.cpp`, `Resolution.cpp`, `Graphics.cpp` — windowed mode and rendering overrides.
- `Storm/` is a link stub for Blizzard's `storm.dll` (see `storm.def`).

Version-check before patching (`isCorrectVersion`); StarCraft version constants live in
`include/starcraftver.h`.

### Static game data and types

`bwapi/BWAPILIB/` is the interface library every bot links. It carries the CRTP type system
(`include/BWAPI/Type.h`) and the big static tables — `UnitType.cpp`, `WeaponType.cpp`,
`TechType.cpp`, `UpgradeType.cpp` etc. define `Type<T, Unknown>::typeNames[]` plus the stat maps.
Adding or changing a type means touching the enum in the header, the name table, and every
per-type stat table in that `.cpp`.

`bwapi/DocumentationGen/` is a console app that links BWAPILIB and *regenerates*
`Documentation/dox/*.dox` (the doxygen tables of unit/weapon/upgrade stats) from those tables, and
also emits unit-type test code. Edit the tables, then re-run it — don't hand-edit the `.dox` files.

### Documentation

`Documentation/Doxyfile` runs over `bwapi/include/` (excluding `include/BWAPI/Client/`) into
`Release_Binary/documentation/`. `Documentation/post-processing/compress.sh` is a bash pipeline
(phantomjs DOM rewrite, htmlcompressor, yuicompressor, pngcrush) that runs afterwards. The
`Documentation` vcxproj is an NMake project that shells out to doxygen from `apps/doxygen/`.

### Other projects in the solution

- `BWAPI_PluginInjector` — Chaoslauncher/MPQDraft plugin that injects `BWAPI.dll`.
- `AIModuleLoader` — client-mode host that loads a bot DLL, so DLL bots can run over the client API.
- `ExampleAIModule` / `ExampleAIClient` / `ExampleTournamentModule` / `DevAIModule` — samples;
  `Release_Binary/ExampleProjects.sln` is what ships to bot authors.
- `BWScriptEmulator` — bot that reimplements Broodwar's built-in AI scripts.
- `SNP_DirectIP` — network provider for direct-IP multiplayer.
- `BWMemoryEdit` — C# WinForms memory inspector (the only non-C++ project).
- `CopyProjects`, `SVNRevGen`, `Installer`, `InstallerArchive` — packaging steps.

### Bot entry points

A DLL bot exports two C functions (see `ExampleAIModule/Source/Dll.cpp`):

```cpp
extern "C" __declspec(dllexport) void gameInit(BWAPI::Game* game) { BWAPI::BroodwarPtr = game; }
extern "C" __declspec(dllexport) BWAPI::AIModule* newAIModule();
```

`GameUpdate.cpp` `LoadLibrary`s the DLL named by `ai`/`ai_dbg` in `bwapi-data/bwapi.ini` and
resolves those symbols. A client bot instead links `BWAPIClient` and calls
`BWAPI::BWAPIClient.connect()` / `.update()` in its own loop.

### Runtime configuration

`Release_Binary/Starcraft/bwapi-data/bwapi.ini` (parsed by `BWAPI/Source/Config.cpp`) selects the
bot DLL, tournament module, auto-menu automation (used for headless/automated matches), and
logging. It is the tracked template for what ships.

## Generated / non-source files

- `bwapi/include/svnrev.h` — generated pre-build by `bwapi/revisionUpdate.vbs` from
  `git rev-list HEAD --count` (+2383, the SVN-era offset). Gitignored; never edit.
- `Documentation/dox/*.dox` — generated by `DocumentationGen` (see above).
- `apps/` — vendored build tools (doxygen, graphviz, Inno Setup, 7z, phantomjs, jars).
- `Release_Binary/` — the release staging tree. Build outputs land here and are gitignored; the
  tracked files (`bwapi.ini`, `ExampleProjects.sln`, vendored Chaoslauncher and MPQDraft binaries)
  are what actually ships.

Version strings live in `bwapi/include/starcraftver.h` (`BWAPI_VER`, `STARCRAFT_VER`) and
`Documentation/Doxyfile` (`PROJECT_NUMBER`) — keep them in sync.

## Conventions

Full rules are in `CONTRIBUTING.md`. The ones that bite:

- **Style:** two spaces, no tabs; Allman braces; space around binary operators but none between a
  function name and `(`; lower camelCase members and methods, UpperCamelCase types, ALL_CAPS
  constants. Loop variables are conventionally `u` unit, `p` player, `b` bullet, `f` force,
  `r` region, `i` index.
- **`#include <Debug.h>` goes last** in a `.cpp`. It `#define`s `new` to `DEBUG_NEW` in debug
  builds, so any header included after it will fail to compile.
- **Vtable stability:** new virtual functions on public interfaces go *at the end* of the class,
  tagged `@since`. Bots are compiled against a released header and dispatch through the vtable, so
  inserting one in the middle silently breaks every existing bot binary.
- **Renaming/deprecating** a public function: keep a non-virtual forwarder with the old name,
  tag `@deprecated`, remove after two minor versions or one major.
- Documentation uses `///` with MSDN `<summary>`/`<param>` XML tags *plus* doxygen `@` commands;
  wrap before column 100.
- Offsets in hex, in `BW/Offsets.h`, guarded by a version check. Avoid inline assembly.
- Prefer `nullptr`, `std::array`, `enum class`, `override`/`final`, in-class member initialisers.

## Branches

`CONTRIBUTING.md` and `README.md` say to develop against `develop`; that reflects upstream
`bwapi/bwapi` history. This fork (`RadicalZephyr/bwapi`) has `main` as its default branch.
