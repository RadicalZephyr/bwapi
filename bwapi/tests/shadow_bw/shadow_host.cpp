// A StarCraft-shaped hole, so BWAPI can be caught in the act of patching it.
//
// tests/bw_layout answers "did the retarget move an address or a layout" by asking the compiler.
// This asks the linked binary instead, and it answers a question the static dump cannot: what does
// BWAPI.dll *do* when it loads? The addresses it patches (BW::BWFXN_*) are constants that never
// appear in a BWDATA reference, the import slots it redirects are decided by the loader, and the
// shape of the hook graph is decided by the linker. None of that is visible in a header.
//
// The trick is that BWAPI does not check it is inside StarCraft. It patches whatever is at
// StarCraft's addresses, and it redirects the import table of whatever process it was loaded into
// (HackUtil::PatchImport passes a null source module, so GetModuleHandleA(nullptr) - the host exe).
// So: reserve StarCraft's address range, fill it with a pattern nothing would write, import the
// storm ordinals and the kernel32/user32 names BWAPI detours, load the DLL, and read back what
// changed.
//
// The report is written to be compared against the same report from a different BWAPI.dll - in CI,
// against upstream's released v4.4.0 binary, which is the only ground truth for "the offsets were
// right before we touched the toolchain". Everything in the report that a rebuild is allowed to
// change is tokenised away: an address inside BWAPI.dll becomes <BWAPI#N>, numbered by order of
// first appearance, so two builds agree if and only if they hook the same places in the same
// pattern. Everything else - which slots, which addresses, how many bytes, what literal values -
// has to match exactly.
//
// Usage: ShadowHost.exe <path-to-BWAPI.dll> <report-path>

#include <windows.h>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "shadow_imports.h"

// StarCraft.exe's image base and a range comfortably past the highest address BWAPI names
// (TileSetMap, 0x006D5EC8). ShadowHost.exe itself is based at 0x20000000 with ASLR off so that
// this reservation cannot collide with it - see ShadowHost.vcxproj.
static const std::uintptr_t SHADOW_BASE = 0x00400000;
static const std::size_t    SHADOW_SIZE = 0x00400000;

// The fill has to make "was this written" answerable exactly, because a byte written with the value
// already there is invisible and a report that sometimes sees a write and sometimes does not would
// diff against itself.
//
// It is answered per aligned four-byte cell rather than per byte: a cell counts as written if any
// of its four bytes moved, so a one-, two- or four-byte patch is always detected - the untouched
// bytes of a partial write give it away - and a full four-byte write would have to reproduce the
// fill exactly, one chance in 2^32, to hide. Byte granularity cannot say that, because a single
// byte of a relative jump's displacement genuinely can equal the fill, and which byte that is moves
// with the DLL's load address.
//
// The window 0x30..0x7F is chosen to be disjoint from every literal ApplyCodePatches writes (0x00,
// 0x04, 0x90) and from the 0xE8 and 0xE9 opcodes, which is what lets the renderer below find a call
// or a jump by looking for its opcode and know it is not looking at fill.
static unsigned char fillByte(std::uintptr_t address)
{
  std::uint32_t h = static_cast<std::uint32_t>(address) * 2654435761u;
  h ^= h >> 15;
  return static_cast<unsigned char>(0x30 + (h % 0x50));
}

static std::uint32_t fillCell(std::uintptr_t address)
{
  std::uint32_t cell = 0;
  for (int k = 3; k >= 0; --k)
    cell = (cell << 8) | fillByte(address + static_cast<std::uintptr_t>(k));
  return cell;
}

// ------------------------------------------------------------------ report

static std::vector<std::string> report;

static void emit(const char *format, ...)
{
  char line[512];
  va_list ap;
  va_start(ap, format);
  vsnprintf(line, sizeof(line), format, ap);
  va_end(ap);
  report.push_back(line);
}

// Any address inside the loaded BWAPI.dll is a build artefact: a different compiler, a different
// inlining decision or a different link order moves it, and none of that is a defect. What is not
// a build artefact is *which* target a given site points at relative to the others, so targets are
// numbered by order of first appearance and compared by that number.
static std::uintptr_t dllBase = 0;
static std::size_t    dllSize = 0;
static std::map<std::uintptr_t, int> dllTargets;

static bool insideDll(std::uintptr_t value)
{
  return dllBase && value >= dllBase && value < dllBase + dllSize;
}

static std::string token(std::uintptr_t value)
{
  char buffer[64];
  if (!insideDll(value))
  {
    std::snprintf(buffer, sizeof(buffer), "0x%08X", static_cast<unsigned>(value));
    return buffer;
  }
  auto it = dllTargets.find(value);
  if (it == dllTargets.end())
    it = dllTargets.emplace(value, static_cast<int>(dllTargets.size()) + 1).first;
  std::snprintf(buffer, sizeof(buffer), "<BWAPI#%d>", it->second);
  return buffer;
}

// ------------------------------------------------------------------ settling

// This cannot use Sleep. BWAPI detours it, and Detours.cpp's _Sleep returns immediately for
// dwMilliseconds == 1500 because that is StarCraft's main menu timer - so a host that slept 1500 ms
// waiting for the persistent-patch thread would not sleep at all, and would report whatever half of
// the patches had landed by then. Waiting on an event nobody signals is not detoured. (CreateEventA
// is; CreateEventW is not.)
static void waitQuietly(unsigned long millis)
{
  HANDLE never = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!never)
    return;
  WaitForSingleObject(never, millis);
  CloseHandle(never);
}

static std::uint32_t digest(const unsigned char *data, std::size_t size)
{
  std::uint32_t h = 2166136261u;
  for (std::size_t i = 0; i < size; ++i)
  {
    h ^= data[i];
    h *= 16777619u;
  }
  return h;
}

// ------------------------------------------------------------------ import table

struct ImportSlot
{
  std::string module;
  std::string symbol;
  DWORD      *slot;
  DWORD       before;
};

static std::vector<ImportSlot> collectImports(HMODULE host)
{
  std::vector<ImportSlot> slots;

  auto base = reinterpret_cast<std::uintptr_t>(host);
  auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(host);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    return slots;

  auto *nt = reinterpret_cast<IMAGE_NT_HEADERS32 *>(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE)
    return slots;

  auto rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
  if (!rva)
    return slots;

  auto *descriptors = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(base + rva);
  for (unsigned d = 0; descriptors[d].Name != 0; ++d)
  {
    const char *moduleName = reinterpret_cast<const char *>(base + descriptors[d].Name);
    // OriginalFirstThunk names the imports; FirstThunk holds the addresses the loader wrote and
    // the ones PatchImport overwrites. They are parallel arrays. A bound-import descriptor has no
    // name array at all, and walking a null one would read the DOS header as though it were thunks.
    if (!descriptors[d].OriginalFirstThunk)
      continue;

    auto *names = reinterpret_cast<IMAGE_THUNK_DATA32 *>(base + descriptors[d].OriginalFirstThunk);
    auto *addrs = reinterpret_cast<DWORD *>(base + descriptors[d].FirstThunk);

    for (unsigned i = 0; names[i].u1.Ordinal != 0; ++i)
    {
      char symbol[256];
      if (IMAGE_SNAP_BY_ORDINAL32(names[i].u1.Ordinal))
        std::snprintf(symbol, sizeof(symbol), "#%u",
                      static_cast<unsigned>(IMAGE_ORDINAL32(names[i].u1.Ordinal)));
      else
        std::snprintf(symbol, sizeof(symbol), "%s",
                      reinterpret_cast<IMAGE_IMPORT_BY_NAME *>(base + names[i].u1.AddressOfData)->Name);

      ImportSlot s;
      s.module = moduleName;
      s.symbol = symbol;
      s.slot   = &addrs[i];
      s.before = addrs[i];
      slots.push_back(s);
    }
  }
  return slots;
}

// ------------------------------------------------------------------ main

static int fail(const char *what)
{
  std::fprintf(stderr, "shadow_host: %s (GetLastError=%lu)\n", what, GetLastError());
  return 1;
}

// ------------------------------------------------------------------ the two roles
//
// StarCraft's address range cannot be claimed from inside a process that has already started. The
// executable is based at 0x20000000 so that its own image is not in the way, but that leaves
// 0x00400000 as ordinary low address space, and the loader hands it to the process heap before a
// single line of this program runs - VirtualAlloc there returns ERROR_INVALID_ADDRESS at the first
// statement of main. Normally StarCraft.exe itself occupies that range, which is exactly why the
// heap goes elsewhere for it and not for us.
//
// So the range is claimed from outside, by a parent. A process created suspended has its image and
// ntdll mapped but has not run its own initialisation, so its heap does not exist yet, and
// VirtualAllocEx at 0x00400000 succeeds. The parent reserves, resumes, and forwards the child's
// exit code; the child is where everything else happens.

static void reportWhatIsInTheWay(HANDLE process)
{
  std::fprintf(stderr, "shadow_host: what is mapped over 0x%08X..0x%08X:\n",
               static_cast<unsigned>(SHADOW_BASE),
               static_cast<unsigned>(SHADOW_BASE + SHADOW_SIZE));

  std::uintptr_t at = SHADOW_BASE;
  while (at < SHADOW_BASE + SHADOW_SIZE)
  {
    MEMORY_BASIC_INFORMATION region = {};
    if (!VirtualQueryEx(process, reinterpret_cast<LPCVOID>(at), &region, sizeof(region)))
      break;

    const char *state = region.State == MEM_FREE    ? "free"
                      : region.State == MEM_RESERVE ? "reserved"
                                                    : "committed";
    const char *kind  = region.Type == MEM_IMAGE   ? "image"
                      : region.Type == MEM_MAPPED  ? "mapped"
                      : region.Type == MEM_PRIVATE ? "private"
                                                   : "-";

    std::fprintf(stderr, "  0x%08X %9u bytes  %-9s %-7s protect 0x%04X\n",
                 static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(region.BaseAddress)),
                 static_cast<unsigned>(region.RegionSize), state, kind,
                 static_cast<unsigned>(region.Protect));

    at = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
  }
}

static int parentMain(const char *dllPath, const char *reportPath)
{
  char self[MAX_PATH];
  if (!GetModuleFileNameA(nullptr, self, MAX_PATH))
    return fail("GetModuleFileNameA");

  char commandLine[MAX_PATH * 3 + 64];
  std::snprintf(commandLine, sizeof(commandLine), "\"%s\" --child \"%s\" \"%s\"",
                self, dllPath, reportPath);

  STARTUPINFOA startup = {};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION child = {};

  if (!CreateProcessA(self, commandLine, nullptr, nullptr, TRUE, CREATE_SUSPENDED,
                      nullptr, nullptr, &startup, &child))
    return fail("cannot start the child process");

  void *shadow = VirtualAllocEx(child.hProcess, reinterpret_cast<LPVOID>(SHADOW_BASE), SHADOW_SIZE,
                                MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  if (shadow != reinterpret_cast<LPVOID>(SHADOW_BASE))
  {
    DWORD why = GetLastError();
    reportWhatIsInTheWay(child.hProcess);
    TerminateProcess(child.hProcess, 1);
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    SetLastError(why);
    return fail("cannot reserve 0x00400000 in the child either");
  }

  ResumeThread(child.hThread);

  DWORD status = 1;
  bool timedOut = WaitForSingleObject(child.hProcess, 120000) != WAIT_OBJECT_0;
  if (timedOut)
  {
    // Leaving it running would leave an orphan holding the report file open.
    std::fprintf(stderr, "shadow_host: the child did not finish within two minutes\n");
    TerminateProcess(child.hProcess, 1);
  }
  else
  {
    GetExitCodeProcess(child.hProcess, &status);
  }

  CloseHandle(child.hThread);
  CloseHandle(child.hProcess);
  return timedOut ? 1 : static_cast<int>(status);
}

static int childMain(const char *dllPath, const char *reportPath)
{
  // The parent committed this before this process ran a single instruction. Check rather than
  // assume: a child started any other way would read four megabytes of whatever is there.
  {
    MEMORY_BASIC_INFORMATION region = {};
    if (!VirtualQuery(reinterpret_cast<LPCVOID>(SHADOW_BASE), &region, sizeof(region)) ||
        region.State != MEM_COMMIT || region.RegionSize < SHADOW_SIZE)
      return fail("0x00400000 is not the committed range the parent should have left here");
  }

  auto *bytes = reinterpret_cast<unsigned char *>(SHADOW_BASE);
  for (std::size_t i = 0; i < SHADOW_SIZE; ++i)
    bytes[i] = fillByte(SHADOW_BASE + i);

  // BWAPI reads this process's own version resource and refuses to apply the version-dependent
  // half of ApplyCodePatches unless it reads 1.16.1 - and pops a modal MessageBox saying so, which
  // in CI is a hang rather than a failure. ShadowHost.rc declares 1.16.1.1; check it here so that a
  // resource that went missing is a diagnosable error instead of a silently smaller report.
  {
    char self[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, self, MAX_PATH))
      return fail("GetModuleFileNameA in the child");

    DWORD unused = 0;
    DWORD size = GetFileVersionInfoSizeA(self, &unused);
    if (!size)
      return fail("this executable has no version resource");

    std::vector<char> data(size);
    VS_FIXEDFILEINFO *info = nullptr;
    UINT infoSize = 0;
    if (!GetFileVersionInfoA(self, 0, size, data.data()) ||
        !VerQueryValueA(data.data(), "\\", reinterpret_cast<LPVOID *>(&info), &infoSize))
      return fail("cannot read this executable's version resource");

    unsigned w1 = HIWORD(info->dwProductVersionMS), w2 = LOWORD(info->dwProductVersionMS);
    unsigned w3 = HIWORD(info->dwProductVersionLS), w4 = LOWORD(info->dwProductVersionLS);
    if (w1 != 1 || w2 != 16 || w3 != 1)
    {
      std::fprintf(stderr, "shadow_host: version resource is %u.%u.%u.%u, need 1.16.1.x\n",
                   w1, w2, w3, w4);
      return 1;
    }
    emit("version %u.%u.%u.%u", w1, w2, w3, w4);
  }

  emit("shadow 0x%08X %u", static_cast<unsigned>(SHADOW_BASE), static_cast<unsigned>(SHADOW_SIZE));

  // Without this the linker drops every import BWAPI wants to detour and PatchImport finds an
  // empty table, which would look exactly like a DLL that hooks nothing.
  referenceDetouredImports();

  HMODULE host = GetModuleHandleA(nullptr);
  std::vector<ImportSlot> imports = collectImports(host);

  // Check the fixture before measuring anything with it. If an import went missing - a linker
  // setting, an edit to shadow_imports.cpp, a storm.def that stopped exporting an ordinal - then
  // PatchImport finds nothing at that slot and says so by omission, and two builds that both hook
  // nothing compare equal. That is the one way this test could fail open, so it is an error and not
  // a smaller report.
  {
    unsigned missing = 0;
    for (unsigned i = 0; i < detouredImportCount; ++i)
    {
      const DetouredImport &wanted = detouredImports[i];
      bool found = false;
      for (const auto &s : imports)
        if (_stricmp(s.module.c_str(), wanted.module) == 0 && s.symbol == wanted.symbol)
        {
          found = true;
          break;
        }
      if (!found)
      {
        std::fprintf(stderr, "shadow_host: %s %s is not in this executable's import table\n",
                     wanted.module, wanted.symbol);
        ++missing;
      }
    }
    if (missing)
      return fail("the fixture is incomplete - see the imports listed above");
  }

  emit("imports %u of %u detoured", static_cast<unsigned>(imports.size()), detouredImportCount);

  // Everything above this line is the fixture. Everything below is BWAPI acting.
  HMODULE dll = LoadLibraryA(dllPath);
  if (!dll)
    return fail("LoadLibrary failed");

  {
    auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(dll);
    auto *nt  = reinterpret_cast<IMAGE_NT_HEADERS32 *>(reinterpret_cast<std::uintptr_t>(dll) + dos->e_lfanew);
    dllBase = reinterpret_cast<std::uintptr_t>(dll);
    dllSize = nt->OptionalHeader.SizeOfImage;
  }

  // ApplyCodePatches runs inside DllMain, so it is already done. The persistent-patch thread runs
  // on a 300 ms cycle and installs the two screen-layer hooks on its first pass; it is idempotent
  // after that, re-reading the layer pointers and leaving them alone once they are its own.
  //
  // Rather than trust one fixed wait to be long enough, wait for the image to stop changing, and
  // for at least five cycles first so that "nothing has happened yet" cannot be mistaken for
  // "everything has happened". A report taken from a still-moving image would be a report that
  // diffs against itself.
  std::uint32_t previous = digest(bytes, SHADOW_SIZE);
  bool settled = false;
  for (int cycle = 0; cycle < 20 && !settled; ++cycle)
  {
    waitQuietly(300);
    std::uint32_t now = digest(bytes, SHADOW_SIZE);
    settled = (now == previous) && cycle >= 4;
    previous = now;
  }
  if (!settled)
    return fail("the shadow image never stopped changing");

  // --- what it redirected -------------------------------------------------
  std::vector<const ImportSlot *> changed;
  for (const auto &s : imports)
    if (*s.slot != s.before)
      changed.push_back(&s);

  std::sort(changed.begin(), changed.end(), [](const ImportSlot *a, const ImportSlot *b) {
    if (a->module != b->module) return _stricmp(a->module.c_str(), b->module.c_str()) < 0;
    return a->symbol < b->symbol;
  });

  emit("");
  emit("# --- import slots redirected ---");
  for (const auto *s : changed)
    emit("import %-16s %-26s -> %s", s->module.c_str(), s->symbol.c_str(), token(*s->slot).c_str());

  // --- what it wrote ------------------------------------------------------
  emit("");
  emit("# --- writes into the shadow image ---");

  unsigned writeCount = 0;
  for (std::size_t cell = 0; cell < SHADOW_SIZE; cell += 4)
  {
    std::uint32_t value;
    std::memcpy(&value, &bytes[cell], 4);
    if (value == fillCell(SHADOW_BASE + cell))
      continue;

    std::size_t start = cell, end = cell + 4;
    while (end < SHADOW_SIZE)
    {
      std::memcpy(&value, &bytes[end], 4);
      if (value == fillCell(SHADOW_BASE + end))
        break;
      end += 4;
    }

    // Everything BWAPI writes here is either a literal or an address inside its own image, and only
    // the second kind is allowed to move between builds - so find those and name them by identity
    // rather than by value.
    //
    // 0xE8 and 0xE9 are neither in the fill nor among the literals, so an opcode byte inside a
    // written run is a real call or jump and the four bytes after it are its displacement. An
    // aligned cell holding an address inside the DLL is a pointer written outright, which is how
    // the persistent-patch thread installs the two screen-layer hooks.
    std::string rendered;
    char piece[64];
    for (std::size_t k = start; k < end; )
    {
      std::uintptr_t at = SHADOW_BASE + k;

      if ((bytes[k] == 0xE8 || bytes[k] == 0xE9) && k + 5 <= end)
      {
        std::int32_t displacement;
        std::memcpy(&displacement, &bytes[k + 1], 4);
        std::uintptr_t target = at + 5 + static_cast<std::uintptr_t>(displacement);
        if (insideDll(target))
        {
          rendered += (bytes[k] == 0xE8 ? " call " : " jmp ");
          rendered += token(target);
          k += 5;
          continue;
        }
      }

      if (at % 4 == 0 && k + 4 <= end)
      {
        std::uint32_t pointer;
        std::memcpy(&pointer, &bytes[k], 4);
        if (insideDll(pointer))
        {
          rendered += " ptr ";
          rendered += token(pointer);
          k += 4;
          continue;
        }
      }

      std::snprintf(piece, sizeof(piece), " %02X", bytes[k]);
      rendered += piece;
      ++k;
    }

    char header[64];
    std::snprintf(header, sizeof(header), "write 0x%08X %-3u",
                  static_cast<unsigned>(SHADOW_BASE + start),
                  static_cast<unsigned>(end - start));
    report.push_back(std::string(header) + rendered);
    ++writeCount;
    cell = end - 4;
  }

  emit("");
  emit("# %u import slots redirected, %u writes, %u distinct addresses inside BWAPI.dll",
       static_cast<unsigned>(changed.size()), writeCount,
       static_cast<unsigned>(dllTargets.size()));

  FILE *out = std::fopen(reportPath, "wb");
  if (!out)
    return fail("cannot open the report for writing");
  std::fprintf(out, "# shadow StarCraft differential report\n");
  std::fprintf(out, "# Generated by tests/shadow_bw/shadow_host.cpp - do not edit by hand.\n");
  for (const auto &line : report)
    std::fprintf(out, "%s\n", line.c_str());
  std::fclose(out);

  std::printf("# shadow StarCraft differential report\n");
  for (const auto &line : report)
    std::printf("%s\n", line.c_str());
  return 0;
}

int main(int argc, char **argv)
{
  if (argc == 3)
    return parentMain(argv[1], argv[2]);
  if (argc == 4 && std::strcmp(argv[1], "--child") == 0)
    return childMain(argv[2], argv[3]);

  std::fprintf(stderr, "usage: ShadowHost.exe <path-to-BWAPI.dll> <report-path>\n");
  return 2;
}
