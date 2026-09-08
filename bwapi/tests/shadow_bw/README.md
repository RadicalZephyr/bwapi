# The shadow StarCraft harness

Tier 2 of the offsets smoke test. It exists because this series retargeted BWAPI's toolset off
`v141_xp` and moved `_WIN32_WINNT` from Windows XP to Windows 7, and BWAPI's entire relationship
with StarCraft is a hundred absolute addresses and a set of structure layouts that have to agree
with a binary nobody here can rebuild.

`tests/bw_layout` answers half of that statically: it asks the compiler what `sizeof` and `alignof`
say and where each `BWDATA` reference is bound, and pins the answers. What it cannot see is
everything that is not a `BWDATA` reference — the `BW::BWFXN_*` patch sites, which are plain
integer constants used only in `CodePatch.cpp`; the import slots BWAPI redirects, which the loader
decides; and whether the compiled DLL actually does at run time what the header says at compile
time.

This asks that instead, of the linked binary, and compares the answer against **upstream's released
BWAPI 4.4.0** — the last build of these offsets that predates the retarget, and the only ground
truth available without a copy of StarCraft.

## How it works

BWAPI never checks that it is inside StarCraft. `HackUtil::PatchImport` passes a null source module,
so `GetModuleHandleA(nullptr)` gives it the **host process's** import table, and `ApplyCodePatches`
writes to StarCraft's addresses whatever happens to be mapped there. So:

1. `ShadowHost.exe` declares a version resource of 1.16.1.1, because `CheckVersion()` reads the host
   executable's version and otherwise skips the version-dependent patches — and raises a modal
   message box, which under CI is a hang rather than a failure.
2. It runs as two processes. StarCraft's address range cannot be claimed from inside a process that
   has already started: the executable is based at `0x20000000` so its own image is out of the way,
   but that leaves `0x00400000` as ordinary low address space, and the loader hands it to the
   process heap before a single line of this program runs — `VirtualAlloc` there fails with
   `ERROR_INVALID_ADDRESS` at the first statement of `main`. (Normally StarCraft.exe itself occupies
   that range, which is exactly why the heap goes elsewhere for it and not for us.) So the parent
   creates the child **suspended** — image and `ntdll` mapped, its own initialisation not yet run,
   no heap — reserves `0x00400000`–`0x00800000` in it with `VirtualAllocEx`, and resumes it. The
   child fills the range with a pattern derived from each address and does everything below.
3. `shadow_imports.cpp` forces the import table to carry the eleven `storm.dll` ordinals and the
   thirteen `kernel32`/`user32` names BWAPI detours, and the host checks that every one of them is
   actually there before it measures anything. Without the references the linker drops the imports,
   `PatchImport` finds nothing, and the report says BWAPI hooked nothing — which would compare equal
   between two builds while proving nothing. That is the one way this test could fail open, so a
   missing import is an error rather than a smaller report.
4. It loads the DLL under test. `ApplyCodePatches` runs inside `DllMain`; the persistent-patch
   thread installs the two screen-layer hooks on a 300 ms cycle, so the host waits.
5. It reads back which import slots moved and which bytes of the shadow image changed, and writes a
   report.

CI runs this against the current build twice and against v4.4.0 once, and requires all three reports
to be identical.

## What a report can and cannot say

Everything in the report that a rebuild is allowed to move is tokenised away. An address inside
BWAPI.dll becomes `<BWAPI#N>`, numbered by order of first appearance — so two builds agree if and
only if they hook the same places in the same *pattern*. A hook that moved to a different site, a
patch that grew or shrank, an import that stopped being redirected, a literal that changed: all of
those fail. Two hooks swapped with each other also fail, because the numbering changes. What does
not fail is the DLL being compiled differently, which is the point.

Writes are detected per aligned four-byte cell, not per byte. A cell counts as written if any byte
in it moved, so a one-, two- or four-byte patch is always caught, and a four-byte write would have
to reproduce the fill exactly to hide. Byte granularity would not do: one byte of a relative jump's
displacement genuinely can equal the fill, and *which* byte moves with the DLL's load address, so
the report would diff against itself. That is why CI runs the current build twice — a difference
there is a defect in this harness, not in either DLL, and it should be diagnosed as one.

The harness does **not** call through the hooks. The detour bodies read game state, and on a
synthetic image they would do arbitrary things. The claim it supports is narrower and still the one
that matters: the same slots are redirected, to the same hook graph, and the same bytes are written
at the same addresses in StarCraft's image.

## The reference pin

`reference.json` pins the release tag, the asset name, and the SHA-256 of both the archive and the
`BWAPI.dll` inside it. CI downloads over the network, so an unpinned reference is not ground truth;
leave a field empty and the job prints what it found, along with the block to paste back, and fails.

This does not move at a submodule pin bump. It moves when we decide to compare against a different
upstream release, which is a deliberate change of reference and not maintenance.

## Running it by hand

From `bwapi/`, with `Storm` and `BWAPI` already built:

```
msbuild tests/shadow_bw/ShadowHost.vcxproj /p:Configuration=Release /p:Platform=Win32
Release\ShadowHost.exe Release_Pipeline\BWAPI.dll report.txt
```

`ShadowHost.exe` is built into `../../Release` deliberately: the loader searches the executable's
own directory first, so BWAPI.dll's `storm.dll` import resolves to the same stub the host already
has loaded.
