# Note: where the "BWAPI successor" idea landed

**2026-09-21.** Zefira and Claude, one session. Status: exploration; no decision recorded.
This note is append-only — add dated sections at the end rather than editing the ones above.

Section references (`§N`) are to `bwapi-c2/docs/c-abi-plan.md` revision 4.5, and `RN` is a
research round under `bwapi-c2/docs/research/`. Paths are in this tree unless prefixed.

## What was asked

Take BWDI's idea — a data-oriented interface to Brood War's internals — the C99 ABI conventions
of bwapi-c2 (§4), and the binary offsets curated in `bwapi/BWAPI/Source/BW/`, and design a
successor to BWAPI, carrying over the shared game rules (`bwapi/Shared/Templates.h`,
`UnitShared.cpp`, `include/BWAPI/Client/CommandTemp.h`) and the static type data
(`bwapi/BWAPILIB/Source/*Type.cpp`) so that nobody re-ports them again.

Mid-conversation the "successor" framing was withdrawn as hyperbole, and a second goal was
added: one BWAPI tree that talks to either retail 1.16.1 or OpenBW as the backend.

## Findings

**1. BWDI is already inside BWAPI.** `include/BWAPI/Client/GameData.h` is BWDI's
`StaticGameData` with the names changed: the same walkability and buildability grids at the
same dimensions, the same map-name buffers with the same "size based on broodwar memory"
comment, the same start-location, selection and region tables, the same unit-finder arrays. The
fossil is `clearanceLevel` — first field of `UnitData`, BWDI's Noticed/Visible/Full concept, and
nothing in this tree reads or writes it. BWDI's `BWDIDriver/BW/Offsets.h` carries this tree's
`BW/Offsets.h` doc comment verbatim. Same hands, same decade. So the server already writes a
pointer-free struct into shared memory every frame, and the OO `Unit`/`Game` classes are a view
over it; JBWAPI, rsbwapi and gobwapi read that struct in place and never touch the C++. The
data-oriented interface exists. What was disliked is a facade over it.

**2. So the "successor" collapses into two projects that already exist on paper**: bwapi-c2 as
the call surface, and a fork of this tree whose one new property is that the same `Server.cpp`
runs against either backend. Nothing in the original ask survives as a third project.

**3. What "data-oriented" could still buy, measured.** A toy ABI
(`2026-09-21-ffi-bench/`), Python 3.11, twenty fields read per unit per frame:

| Frame shape | 200 units | 1,700 units |
|---|---|---|
| Per-field ctypes calls | 2,485 µs | 18,399 µs |
| One snapshot call, then ctypes field reads | 334 µs | 2,976 µs |
| In-place mmap view, ctypes field reads | 332 µs | 2,909 µs |
| Snapshot memcpy alone | 3 µs | 21 µs |
| Snapshot, then numpy vectorised reads | 8 µs | 46 µs |

The copy is one percent of the frame, so reading the mapping in place buys nothing over a
snapshot in Python, and a call surface loses nothing as long as snapshots exist — §5.10 stands.
The per-field-call shape is seven times slower and caps a Python bot near 400 frames per second:
fine for retail at 24, hopeless for OpenBW headless. A record that a structured dtype can view
directly is another forty times cheaper, and that is the only place a layout "designed for how
bots read it" pays. The toy has no `resolve()` and no virtual call behind each getter, so the
real gaps are wider. **The data-oriented ambition is therefore a refinement of §5.10** — a
padding-free record that numpy and a C# `Span<T>` can view, and snapshots for whatever
per-frame state still lacks one — not a new layout and not a new project.

**4. The two backends already share the wire format.** R7 §5 diffed the headers: `GameData.h`
is byte-identical between upstream 4.4.0 and OpenBW's fork; 13 public headers differ by 180
lines; `CLIENT_VERSION` is 10003 upstream and 10002 on the fork; `CommandTemp.h` is absent on
the fork, so there is no latency compensation there. OpenBW's fork kept the whole `Server.cpp`
population loop running every frame as its own internal state and gutted only the transport
(R6 §13). The seam between the backends is `BWAPI/Source/BW/`: offsets into a 1.16.1 process on
one side, `OpenBWData/BW/BWData.cpp` calling into the engine on the other. Everything above
that line is common code today.

**5. Client mode is the mode in which one bot serves both ladders.** A client bot is its own
process: it is run, maps `GameData`, and does the two-byte handshake on the named pipe (retail)
or the `AF_UNIX` socket (OpenBW, `basil-ladder/bwapi@linux-client-support`, ~344 lines).
Because the layout is identical, the same bot source attaches to either server, and a Python or
C# bot is the same *file* on both — only `bwapi_c2` is built per platform. Module mode can never
offer this: its interface is `AIModule`'s C++ vtable in the host's compiler ABI at the host's
bitness, x86 MSVC on retail and x86-64 GCC on OpenBW. Two conditions. A client transport per OS:
seven Win32 imports in `BWAPIClient/Source/Client.cpp` to port (Appendix B). And the version
gate at `Client.cpp:120` relaxed, because 10002 ≠ 10003 while the layout is the same anyway;
JBWAPI skips the check and runs on both ladders, gobwapi hardcodes 10003 and cannot. Compare the
mapping's size to `sizeof(GameData)` at connect instead: that catches a real layout change,
which the constant does not — the last change to `GameData.h`, `randomSeed` in August 2016,
did not bump it.

**6. OpenBW is a tournament target, not a development-time backend.** BASIL's always-on ladder
runs submitted bots on OpenBW via the `basil-ladder` forks; SSCAIT runs retail 1.16.1. So
bug-accuracy between the backends is a hard requirement of the fork, not a nicety, and the
acceptance test is a replay played on both with `GameData` diffed frame by frame. Visibility,
detection, latency frames and the seed are where silent bot bugs would live.

**7. Module mode should go away.** Zefira's position, recorded with its reasons: it is the one
mode that cannot be backend-agnostic (finding 5); its v2 motivation in Appendix A, grouped
commands, is weaker than the plan states (finding 8); and its in-process speed advantage on
OpenBW is the difference between a socket round trip and a function call, small against the
frame budgets in finding 3. Not a decision yet — Appendix A stays a scoped v2 item in bwapi-c2
until that plan is revised.

**8. Two corrections to carry back to the bwapi-c2 plan.**

- Appendix A overstates the grouped-command gap. `BWAPI/Source/BWAPI/CommandOptimizer.cpp`
  merges identical single-unit commands into 12-unit selections at optimisation level 1 and up,
  and client commands reach it through the same `UnitImpl::issueCommand` path as module
  commands (`Server.cpp:848` → `UnitImpl::prepareIssueCommand` → `commandOptimizer.add`). What
  a client cannot do is *express* a group; it does benefit from one. Grouped commands are a
  nice-to-have, if cheap.
- §4 rejects runtime version selection because the dependency never moves. Two backends are a
  form of drift, so the premise weakens, but the answer does not change: the size prefix that
  survived in §4 is the right mechanism, and the connect-time mapping-size check in finding 5 is
  the backstop. Still no FoundationDB-style selection.

**9. What survives of BWDI as an idea.** One field: a single clearance level per unit record
saying how much of it may be trusted, in place of nine visibility booleans and a detected flag.
Worth a field in the §5.10 snapshot, nothing more. Its per-frame add/remove lists are BWAPI's
events; its shared-memory stacks are BWAPI's shapes and commands arrays.

## Alternatives considered

| Alternative | Why not |
|---|---|
| Rewrite the injected server against the `BW/` offsets with a new layout | 1.16.1 forever and x86 forever; nothing testable without a Windows install of the game; tournaments would have to trust a new anti-cheat; and the layout it would produce already exists (finding 1) |
| A new data layout as the public ABI, read in place, JBWAPI-style | No gain over a snapshot in Python (finding 3); gives up the C++ rule engine bwapi-c2 exists to keep; struct-of-arrays is second-order once a record is vectorisable |
| Port `Templates.h` and the type data to C99 over the data layout | Only needed by the alternative above. Every prior port diverged (§2, non-goal 1). If it were ever done, the differential test is already available: the bwapi-c2 suite runs the real closure on Linux over synthetic `GameData` |
| OpenBW as public CI | Needs eleven Blizzard tables (R7 §2). Stays a local substrate; fixtures stay CI. One experiment left open below |
| Fix grouped commands in a new server | Nice-to-have (finding 8) |

## Where we landed

- bwapi-c2 continues as planned. The data-oriented wish is a refinement of §5.10 when phase 2
  gets there.
- The fork of this tree is a BWAPI project, upstream of bwapi-c2's Appendix B: one tree, two
  `BW/` backends selected at build time, one `Server.cpp`, a POSIX transport on the server and
  on the client, and the version gate replaced by a size check. Its cost is the rebase: OpenBW's
  fork is a 2016-era BWAPI, five years behind, pinned to an engine fork with a broken headless
  stub (R6 §13). Its acceptance test is the replay diff of finding 6.
- Sequencing: phases 2 to 4 of bwapi-c2 do not depend on the fork. The one reason to pull it
  earlier is phase 3's live run, manual on Windows today, which would become local and
  repeatable on Linux.

## Open questions and next experiments

1. **Replay diff across backends.** The acceptance test for "in line" (finding 6). Needs the
   fork built for both backends and a replay both can play; the diff itself is a small program
   over two `GameData` streams.
2. **Synthetic `.dat` tables.** R7 §2 says OpenBW's `data_files_loader` is a template that the
   fork never exposes. If a melee game runs on made-up tables, a real engine runs in public CI.
   `iscript.bin` is the likely blocker; it drives attack timing.
3. **What BASIL actually runs.** Pin the fork's OpenBW target to the `basil-ladder` commits the
   ladder deploys, not to `OpenBW/bwapi@develop-openbw`.
4. **Latency compensation on OpenBW.** `CommandTemp.h` is absent on the fork. Either the fork
   carries it for both backends or `bwapi_game_set_lat_com()` is a no-op there, and the header
   should say which.

## Sources

- This tree: `include/BWAPI/Client/GameData.h`, `UnitData.h`;
  `BWAPI/Source/BWAPI/UnitUpdate.cpp`, `CommandOptimizer.cpp`, `Server.cpp`, `UnitImpl.cpp`;
  `BWAPI/Source/BW/CUnit.h`, `Offsets.h`.
- BWDI mirror: `BWDI/StaticGameData.h`, `UnitState.h`; `BWDIDriver/Driver/Engine.cpp`;
  `Bridge/*.h`.
- bwapi-c2: `docs/c-abi-plan.md` §1.4, §4, §5.10, §6, Appendix A, Appendix B;
  `docs/research/r5-x64-settled.md`, `r6-link-closure.md` §8 and §13,
  `r7-openbw-ci-substrate.md` §2, §4, §5.
- The benchmark: `2026-09-21-ffi-bench/abi.c`, `bench.py`.
