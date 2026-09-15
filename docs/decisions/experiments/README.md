# Experiments

Experiments backing the decision records in
[`docs/decisions/records/`](../records/).

Any code that produces concrete data used to argue an ADR lives here rather
than in a scratch file or a gist. An ADR that cites a measurement is only as
good as a reader's ability to re-run it, and a benchmark that lived in
someone's working tree cannot be re-run at all.

## Layout

This directory is a sub-project of the main build. It depends on the project by
path rather than on a published version, and is never published itself. The
template this came from also has it covered by the same lint, type and test
checks as the rest of the repository; **this fork has none of those to be
covered by** -- upstream BWAPI runs no CI, and `cppcheck_script.bat` is a manual
step over the module sources. So the one check an experiment here gets is that
it is part of the default build and therefore has to compile, which is why it is
built by default rather than behind an off-by-default option. How it joins the
build, and what it is called, the project says here:

> **Sub-project:** a CMake subdirectory of this fork's top-level project, added
> by `add_subdirectory(docs/decisions/experiments)` from the root
> `CMakeLists.txt`. It has no name of its own beyond the targets in it; it is
> built whenever BWAPI is configured as the top-level project, and switched off
> with `-DBWAPI_BUILD_EXPERIMENTS=OFF`.

That root `CMakeLists.txt` is **this fork's**, not upstream's. Upstream
`bwapi/bwapi` has no top-level CMake project at all -- [`CMake/`](../../../CMake)
holds two subdirectories a bot's own build `add_subdirectory()`s, which is what
[`CMake/README.md`](../../../CMake/README.md) documents. The root file exists so
that this directory has a build to join, and it is deliberately thin for the
same reason.

Each experiment is one entry point named after the ADR it serves:

```text
docs/decisions/experiments/0001-some-decision.cpp
```

declared in [`CMakeLists.txt`](CMakeLists.txt) with one line --
`bwapi_add_experiment(0001-some-decision)` -- and run from the repository root
with one command:

```shell
cmake --build build --target run-0001-some-decision
```

after a one-time `cmake -B build`. The `run-` target builds the entry point and
executes it; the bare target name builds it without running.

## What builds where

BWAPI is Windows-only, 32-bit and MSVC-only, and **nothing in it builds or runs
on Linux** -- [`CLAUDE.md`](../../../CLAUDE.md) opens with that and it is the
single most important fact about working here. It constrains this directory
directly, so `bwapi_add_experiment` takes an argument saying what the experiment
needs:

| Declared as | Links against | Builds on |
| --- | --- | --- |
| `bwapi_add_experiment(NNNN-name)` | `bwapi_headers`, an interface target carrying `bwapi/include` and `bwapi/Util/Source` | anywhere, including Linux/clang |
| `bwapi_add_experiment(NNNN-name CLIENT)` | the client-mode static libraries from `CMake/` as well | Windows/MSVC only |

**Default to the header-only form.** An experiment that reaches for `CLIENT` has
narrowed its audience to whoever has a Windows box, so it should be reaching for
something only the implementation can tell it.

### An experiment here measures the host, not StarCraft

The trap is specific enough to state as a rule. A header-only experiment
compiles with *your* compiler for *your* target, so every number it prints is a
fact about the host ABI. StarCraft is Win32. Those are not the same ABI, and a
`sizeof` copied out of one into a record about the other is simply a wrong
number.

The uncomfortable part is that they often agree, which is what makes it a trap
rather than an obvious mistake. `sizeof(GameData)` is 33,017,048 under
`i386-pc-windows-msvc` and also 33,017,048 under `x86_64-unknown-linux-gnu`, so
an experiment run on a Linux box prints the right answer -- while
`i386-unknown-linux-gnu` gives 33,016,644, because `BulletData` is 76 bytes
there instead of 80 and every field after `players` shifts. That agreement is a
**measured contingent fact**, not a property of the struct: it was measured, in
[bwapi-c2's R5](https://github.com/RadicalZephyr/bwapi-c2/blob/main/docs/research/r5-x64-settled.md),
across six targets precisely because nobody could safely assume it.

So: **state the target triple beside any number an experiment prints**, the same
way [`../README.md`](../README.md#dating-claims-that-will-not-age-well) says to
state the date beside a benchmark. It is the same rule -- a number that is true
*as of* something has to say as of what -- and here the something is a target
rather than a date.

### Where an MSVC ABI question actually goes

Not here. Two routes beat this sub-project for anything about Win32 layout, and
both are worth knowing before reaching for `bwapi_add_experiment`:

- **Compiler Explorer carries real MSVC builds**, which no Linux checkout has.
  A layout question put to actual `cl.exe` needs no reproduction and no
  cross-compilation trick, and the compiler picker names the version for the
  provenance line. This is the *first* thing to try for an ABI question, not the
  fallback. [`../README.md`](../README.md#what-the-playground-has-to-be) has the
  route and the four parts such a record carries.
- **Cross-target `-fsyntax-only` with a self-contained reproduction**, which is
  how R5 built the table quoted above. Worth understanding why it needs the
  reproduction: `clang++ -target i386-pc-windows-msvc` cannot include BWAPI's
  headers at all, because it then wants MSVC's standard library and a Linux box
  does not have one -- `Position.h` fails on `#include <cmath>` before anything
  interesting happens. R5 reproduced the structs standalone for exactly that
  reason, and read the sizes out of a deliberate `undefined template 'SZ<N>'`
  diagnostic because a syntax-only build produces no program to run.

What is left for this directory is the class of question that needs BWAPI's
*real* headers or its sources -- what a `Shared/` translation unit does against
both `UnitImpl` definitions, whether a field exists, what a template expands to
-- and can live with host-ABI numbers, or needs none.

An experiment that needs neither the headers nor the libraries -- only the
compiler and the standard library -- does not belong here at all. *What does not
go here*, below, says where it goes instead.

The record's name is the only thing that varies from one experiment to the
next. Print results in whatever shape the ADR needs to quote them, and link the
entry point from the section that relies on the numbers. Fixtures shared between
experiments -- a data builder two of them both need, a timing harness -- get a
shared module, and it stays empty until something is actually shared. Resist
putting an experiment's own scaffolding there.

## What does not go here

Research produces *evidence*, not tests. It is not asserting that the project
is correct, and it is not expected to keep passing -- it answers a question
that was open at the time an ADR was written.

An experiment that needs nothing from this project does not belong here either.
If it depends only on the language's toolchain and standard library -- a probe
into type inference, a diagnostic worth quoting -- it goes in a playground share
link recorded in the ADR instead, which is also the only route available to a
case that has to *fail* to build. That holds while
[`../README.md`](../README.md) names a playground; where it names none, a
toolchain-only experiment that builds lives here after all, and a case that has
to *fail* to build is recorded in the ADR with no link. The same file has the
routing rule, what the playground has to provide, and the version stamp such a
record has to carry.

## Retirement

An experiment is maintained while the decision it serves is still being argued
or built. It leaves the moment that decision is done -- which the record dates
exactly, in the `Implemented` row of its status log, or in the `Deprecated` row
if the decision was withdrawn instead of built -- through one of two exits:

- **Deleted** -- it measured internals the ADR replaced. An entry point that
  no longer builds against the new code is deleted rather than repaired; cite
  the commit that produced the numbers in the ADR and history keeps it. Most
  records here argue for changing the internals an experiment was measuring, so
  breaking is how it ends rather than a regression.
- **Promoted** -- it still answers a live question, which means it stopped
  being research. A measurement worth re-running is a benchmark and moves to
  the benchmark suite; something asserting a property we promise is a test and
  moves to the test suite. A project with no benchmark suite starts one the
  first time this happens; the experiment does not stay here as the substitute.

What an experiment never does is linger. This sub-project is a **staging area,
not an archive**: everything in it has a scheduled exit, and a healthy one
trends toward empty. Accumulation is the signal to look for something
miscategorised -- a benchmark that was never promoted, or an experiment whose
ADR quietly landed months ago.
