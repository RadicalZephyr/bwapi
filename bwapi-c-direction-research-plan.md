# Research plan: deciding what this library should be

Revision 3 of the plan is a good design for a project whose premises have not been checked.
A survey turned up three facts that bear directly on those premises:

- **A `bwapi-c` already exists.** RnDome/bwapi-c reached v1.0, wrapped nearly all of BWAPI,
  supports module *and* client mode, builds on Windows with MSVC and Linux with GCC, and has
  downstream `bwapi-sys` / `bwapi-rs` crates on crates.io. It is dormant, roughly eight years
  idle, with dead CI.
- **The two languages we scoped as in-scope are already served**, by the approach non-goal 1
  rejects. rsbwapi is a maintained Rust BWAPI client (last release October 2024) with a
  competitive bot behind it; JBWAPI is a pure-Java client reading the memory-mapped file
  directly, with no external DLL, supporting 32- *and* 64-bit Java.
- **OpenBW exists.** OpenBW maintains a BWAPI 4.2.0 fork that builds with CMake and runs
  headless on Linux, and BWAPI4J runs against it in 64-bit.

Each of those undermines something the plan currently assumes. The purpose of this research is
to settle the premises before phase 0, not to refine the design further. Nothing here is design
work, none of it requires writing the library, and the whole thing is desk work plus two short
build experiments.

**Constraint: no community outreach.** Asking the bot-development community directly is the
cheapest signal available and it is deliberately excluded here, so every demand question below
has to be answered from public artifacts instead.

---

## Ordering

Cheapest-and-most-decision-changing first. Stop early if an answer forecloses the rest.

| # | Item | Timebox | Blocks |
|---|---|---|---|
| R1 | Audit `RnDome/bwapi-c` | 2–3 h | Fork vs. greenfield |
| R2 | Size the real API surface from bot corpora | 3–4 h | Whether the generator is needed at all |
| R3 | Demand evidence from public artifacts | 2 h | Whether to build this at all |
| R4 | Study rsbwapi and JBWAPI as the competition | 3 h | The rev 4 rationale; which languages are unserved |
| R5 | Settle x64 from existing implementations | 1–2 h | Deletes most of §10.2 |
| R6 | Derive the link closure by building it | half day | §10.1, and confirms R5 |
| R7 | Evaluate OpenBW as the CI substrate | half day | Whether the mock server is needed |
| R8 | `swig -c` experiment on one header | half day | Whether §9's generator is ours or SWIG's |
| R9 | Licensing verification | 1 h | §0, and fork compatibility |
| R10 | Name availability | 30 min | §3, §10.4 |

Roughly three days of work, and R1–R3 alone (a day) may be enough to change the direction.

---

## R1. Audit `RnDome/bwapi-c`

`https://github.com/RnDome/bwapi-c` — 148 commits, v1.0, 20 stars, 1 fork, 2 open issues.

Read `include/`, `src/`, `CMakeLists.txt`, `FindBWAPI.cmake`, and both `example/Dll.c` and
`example/Client.c`.

**Answer these:**

- How many exported entry points, and how were they produced — generated or hand-written? If
  generated, from what?
- What is the object model? The README's example uses a `Broodwar` handle passed to
  `Game_registerEvent`, which suggests opaque pointers rather than integer IDs. Confirm, and
  find out whether anything validates them.
- How does it return collections, strings, and positions? Release 0.3 mentions iterator objects
  (`PositionIterator`, `UnitTypeIterator`, `EventIterator`) yielding pointers to plain C
  structs — that is a third model, neither our caller-buffer convention nor a snapshot.
- What is the error story? Is there one?
- How does the client-mode path work, and is it complete? The README says client mode compiles
  but is unsupported on OpenBW.
- Is there a `Filter` API in C, and what shape did it take? Release 0.2 claims one.
- What is the license, and is it compatible with forking under LGPL-3.0?
- Read the two open issues and the open PR. Those are the field reports we cannot get by asking.

**Deliverable:** a table with one row per rev 3 §4 convention (handles, booleans, positions,
strings, collections, errors, structs, ordering, re-entrancy) and columns for "what bwapi-c
does" and "what we would do differently, and why it is worth the divergence."

**Decides:** whether the project is a fork, a rewrite with bwapi-c as the reference
implementation, or a greenfield build. A fork inherits ~900 wrapped declarations and a working
CMake build; the cost is inheriting an object model we have already argued against.

---

## R2. Size the real API surface

§1.8 derives ~900 declarations collapsing to 550–600 entry points, and §9's entire generator
argument rests on that number. But 600 is a *completeness* target, not a usage-driven one, and
the phase 5 exit criterion — a C99 bot that reads state and moves units — needs perhaps 60
functions.

**Method:** clone a corpus of open-source BWAPI bots and count call sites.

- ZZZKBot, UAlbertaBot, OpprimoBot / RazeAndPlunder, Steamhammer, the BWAPI ExampleAIModule and
  ExampleAIClient, and Styx2 (Rust, via rsbwapi — a useful cross-check on what a non-C++ bot
  actually needs).
- Grep for `Broodwar->`, `->getX()`, `Filter::`, `UnitTypes::`, and the `canXxx` family; rank by
  distinct call-site count and by number of bots using each.

**Answer these:**

- What is the smallest set of entry points that covers, say, 95% of call sites across the
  corpus? 150? 250?
- How many of the ~130 `canXxx` predicates are ever called? My prior is "a handful."
- How many of the ~90 draw calls are used, and in which coordinate spaces?
- How much of the surface is static type data (§5.8), and is it accessed as functions or as
  lookup tables the bot builds once?

**Deliverable:** a frequency-ranked function list, and a proposed v1 cut line.

**Decides:** whether a spec DSL, emitters, `api.json`, and a coverage audit are load-bearing or
are consequences of a completeness goal nobody requested. If the 95% line is under ~250
functions, the generator becomes optional and the roadmap collapses by several phases.

---

## R3. Demand evidence from public artifacts

Without asking anyone, establish whether there is an audience.

**Sources:**

- crates.io download counts: `bwapi-sys` (~6.1k all-time over eight years), `bwapi` (~3.4k),
  `rsbwapi`, `bwapi_wrapper`. Compare shapes over time.
- The SSCAIT and BASIL bot ladders publish bot lists; many entries state the language and
  framework. Count bots by language and by binding library.
- Issue trackers on `bwapi/bwapi`, `rsbwapi`, `JBWAPI`, `BWAPI4J`, and `bwapi-c`: search for
  requests for Python, C#, Zig, JavaScript, or "C API" bindings. Count and date them.
- GitHub code search for files importing `bwapi-sys` or `BWAPIC.h`.

**Answer these:**

- How many bots exist outside C++ and Java, and what do they use?
- Is there a visible unserved language, or is the unserved set hypothetical?
- Has anyone tried and abandoned a Python/JS binding, and why?

**Deliverable:** one paragraph and a table. If the honest answer is "the unserved audience is
two people," that is the most valuable finding in this document and it should reshape scope
rather than be buried.

---

## R4. Study rsbwapi and JBWAPI as the competition

Non-goal 1 says per-language protocol reimplementation means re-implementing `Templates.h`
(3,098 lines of game rules) and `CommandTemp.h` in every language, and calls it a trap. Two
live projects did it anyway. Test the claim.

**Method:** read `Bytekeeper/rsbwapi` and `JavaBWAPI/JBWAPI`.

- Measure how much of each is a port of BWAPI's shared game-rules code versus protocol
  plumbing. Look specifically for ports of `canBuildHere`, `canMake`, the `canXxx` family,
  `hasPower`, `iterateUnitFinder`, and latency compensation.
- Check how each handles the `unitFinder` spatial index and whether they reproduce or skip it.
- Check their per-frame data strategy. BWAPI4J states that it minimizes JNI calls by caching
  per-frame data in a single large array copy — the same conclusion as §5.10, reached
  independently and under measurement. Find out whether rsbwapi does something similar.
- Read their issue trackers for the maintenance costs the trap claim predicts: bugs where the
  reimplementation diverges from BWAPI's own rules.

**Answer these:**

- Is the trap real, and if so, what does it cost in practice?
- What do they *not* cover that a C ABI over the real `BWAPI::Game` would get for free?
- Does the existence of a good Rust option mean Rust should be dropped from our in-scope
  bindings, leaving the ABI to serve languages with no option at all?

**Deliverable:** the honest paragraph that replaces non-goal 1's assertion with an argument.

---

## R5. Settle x64 from existing implementations

§10.2 sequences a two-stage proof: layout dumps at phase 0, cross-bitness interop at the
mock-server phase. JBWAPI has been doing cross-bitness interop in production for years — a
64-bit JVM maps the same `Local\bwapi_shared_memory_<pid>` region a 32-bit server writes and
reads `GameData` out of it.

**Method:** read JBWAPI's client and `GameData` accessor code, and BWAPI4J's bridge.

**Answer these:**

- Does JBWAPI apply any bitness-dependent offset handling, or does it treat the layout as
  fixed? Fixed is the existence proof.
- Are there documented caveats — fields it skips, alignment it works around?
- BWAPI4J requires 64-bit for OpenBW and 32-bit for retail BW. Why? Is that a BWAPI constraint
  or a JNI/bridge one?

**Decides:** whether §10.2 survives at all. Expected outcome: the layout question is already
answered empirically, the generated layout dump drops from a phase gate to a cheap regression
check, and the "final verdict at phase 3" ceremony disappears.

---

## R6. Derive the link closure by building it

§10.1 asserts a client-only source set and excludes Storm, Util, and two Boost-touching
translation units. That is reasoning from greps. Confirm it with a linker.

**Method:**

1. Build `BWAPILIB/Source` + `BWAPIClient/Source` + `Shared` from a pinned BWAPI as a static
   lib, x86, with explicit file lists. Record every undefined symbol.
2. Repeat for x64. Record what changes.
3. Grep the whole tree for `StructMemberAlignment`, `/Zp`, and `#pragma pack`, confirming that
   Storm is the only offender and that it is outside the closure.
4. **Read OpenBW's CMake first.** The OpenBW BWAPI fork already builds BWAPI cross-platform
   with CMake, which means someone has already solved the "what does a non-injected BWAPI
   actually need" problem. Their file lists may make step 1 unnecessary.

**Answer these:**

- Does anything in the client path genuinely pull a Storm symbol? If it does, x64 is dead and
  §10.2, §13, and the Node story all change.
- Does dropping `Streams.cpp` and `BroodwarOutputDevice.cpp` actually remove Boost?
- Do the two `clientInfo` pointer↔`int` casts instantiate?

---

## R7. Evaluate OpenBW as the test and CI substrate

§11.5 budgets a hand-written mock server — the largest single line item in the roadmap, and I
would size it at 800–1500 lines with scenario fixtures, not the 300 the plan assumes. OpenBW is
a real headless BWAPI that runs on Linux, and JBWAPI and BWAPI4J both use it.

**Method:** clone `OpenBW/openbw` and `OpenBW/bwapi`, build, and try to run a trivial bot
headless.

**Answer these:**

- Does it still build and run on a current toolchain? Both repos have recent commits.
- **Does it need the retail MPQ files?** It does — `StarDat.mpq`, `BrooDat.mpq`, `Patch_rt.mpq`.
  That is a licensing problem for public CI: those cannot be committed or downloaded. Find out
  how JBWAPI and BWAPI4J's CI handles it, or whether their CI simply does not run games.
- Does *client mode* work against OpenBW, or only module mode? bwapi-c's README says client mode
  is unsupported there, which if still true removes most of the benefit for us.
- OpenBW's BWAPI fork explicitly no longer works with retail Brood War. So supporting both means
  two build configurations and two pinned dependencies. Price that.
- Is there a `mini-openbwapi` path (it exists in the OpenBW tree) that is a smaller target than
  full BWAPI?

**Decides:** whether the mock server is built, replaced, or reduced to a thin fake used only for
error-path tests. Also whether non-goal 3 ("not cross-platform") survives.

---

## R8. The `swig -c` experiment

§1.6 and §9 reject SWIG on the grounds that it emits a per-language C++ layer. **For SWIG's C
module that premise is false** — `swig -c` targets C itself and aims to emit a pure ISO C
interface to a C++ library. The rejection has to be rewritten regardless of the outcome, so
establish the outcome.

**Method:** run SWIG 4.3 with `-c++ -c` against `include/BWAPI/UnitType.h` first, then
`include/BWAPI/Unit.h`.

**Answer these:**

- Does SWIG parse BWAPI's headers at all, given MSVC extensions and `Templates.h`?
- The documented shortcomings list enums with class or namespace context as broken. Does that
  actually take out `UnitTypes::Enum`, `Orders::Enum`, and the rest — i.e. the ~700 constants of
  §5.8, the highest-value block in the ABI?
- What shape does `BWAPI::Type` come out as? Does `constexpr operator int()` survive, or does
  every type become an opaque `SwigObj*`?
- What happens to `std::string` returns, `Unitset` returns, `std::function` filters, and
  printf-style varargs (documented as unsupported)?
- Can typemaps express *any* of: caller-provided buffers with true-count returns, sorted output,
  packed `int64_t` positions, the sticky error latch, size-prefixed structs?

**Decides:** whether §9's generator is ours or SWIG's. My expectation is that it stays ours, on
four grounds rather than the current wrong one — module still under development, opaque pointers
instead of integer handles, namespaced enums broken, and none of the §4 conventions expressible.
But that should be written down with evidence, not asserted.

**Fallback finding worth capturing either way:** even if SWIG is rejected, libclang or CastXML
can generate the *first draft* of the YAML spec rather than us hand-typing 900 entries. That is
the same parser `check_coverage.py` needs, run in the other direction, and it is the realistic
tooling win.

---

## R9. Licensing verification

§0 asserts LGPL-3.0 and builds three design constraints on it. Verify rather than assume.

- Read BWAPI's `LICENSE` and `LICENSE.md` directly and confirm the version and any exceptions.
- Read `RnDome/bwapi-c`'s license — if we fork it, compatibility is a precondition.
- Read OpenBW's license, which may differ from BWAPI's and which matters if OpenBW becomes a
  build target.
- Confirm the LGPL-3.0 System Libraries exception covers the MSVC static CRT for our
  distribution, and note what a release asset must contain.
- Note the MPQ problem from R7: those are Blizzard's, cannot be redistributed, and constrain
  both CI and any "just run the example bot" onboarding story.

---

## R10. Name availability

- `bwapi-c` is taken as a GitHub repo name by RnDome and is the name their published crates
  reference.
- **`bwapi-sys` is taken on crates.io**, published by RnDome roughly eight years ago. §10.4
  assumes we can publish it. Check crates.io's name-transfer and squatting policy, and pick a
  fallback.
- Check `bwapi`, `bwapi_wrapper`, and `rsbwapi` — the obvious Rust names are largely gone.
- Check npm for the JS side.

**Deliverable:** a naming decision that replaces §3's table, and a note in §10.4.

---

## Decision framework

The research above should resolve to one of three directions. Stating them now keeps the
research honest — each finding should move probability between these rather than generating
more design.

| Direction | What it means | Findings that support it |
|---|---|---|
| **A. Revive** | Fork `RnDome/bwapi-c`, modernize under rev 3's §4 conventions, fix CI, republish | R1 shows a sound build and a wrappable object model; R9 shows license compatibility; R2 shows the existing surface is close to what bots need |
| **B. Rebuild, small** | Greenfield, ~200 hand-written functions at rev 3's conventions, OpenBW or a thin fake for CI, generator only if the tail materializes | R1 shows the object model is not salvageable; R2's 95% line is low; R7 gives a usable CI substrate |
| **C. Don't** | Skip the C ABI; write the specific unserved binding (Python? Zig?) directly, or contribute upstream | R3 shows no audience beyond the served languages; R4 shows the reimplementation trap is not costly in practice |

**Bias to record up front:** the plan as written is direction B-with-a-generator scaled to
completeness, and much of its machinery — declaration hashes, `api.schema.json` versioning, a
maintained divergence register, a six-step pin-bump checklist, an eight-phase roadmap — is
insurance against upstream motion in a dependency that has not moved in years, some of it added
on my own recommendation. If R2 and R3 come back small, the correct response is to cut, not to
build the same thing more carefully.

**What is not in question**, whichever direction wins: the §4 conventions. Handles as integer
IDs, caller-provided buffers, sticky first-error latching, packed positions, size-prefixed
structs, sorted output, per-frame snapshots, and the C99-bot exit criterion are cheap to decide
now and expensive to retrofit. They are the actual product; the entry-point count is not.
