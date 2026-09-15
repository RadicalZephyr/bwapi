# Running BWAPI Bots Locally on Linux: Provenance, Legality, and What Actually Works

**Status:** Research findings — draft
**Date:** 2026-09-15
**Context:** Getting a local Linux dev loop for `bwapi-c2` and for iterating on BWAPI's protocol.

---

## Verification legend

This document mixes things I ran myself with things I read. The distinction matters, so
every non-obvious claim is tagged:

- **[V]** Verified — I executed the command or read the source. Reproduction steps in Appendix A.
- **[S]** Secondhand — sourced from documentation, a wiki, or a court summary. Cited, not independently confirmed.
- **[?]** Open — I could not close this remotely. Needs a local experiment.

---

## Summary

1. `Battle.net-Setup.exe` is a 4.9 MB bootstrapper. It contains no game data. **[V]**
2. More importantly: the StarCraft product Blizzard ships today contains **no `.mpq` files at all**.
   I pulled and decoded Blizzard's own install manifest to confirm this. The game moved to CASC. **[V]**
3. Therefore the plan "install free StarCraft, take the three MPQs, feed them to OpenBW" **does not
   work as stated** against the current client. OpenBW's "1.16.1 or 1.18" documentation is accurate
   but dated — patch 1.18 (2017) was MPQ-based; the product is now at 1.23.10 and is not.
4. OpenBW hard-requires MPQ containers. It has no loose-file loader. But its loader is a template
   with a one-method interface, so a directory-backed replacement is small. **[V]**
5. The headless engine reads only about ten small data tables. The bulk art and sound payload is
   UI-only. This makes a CASC-extraction workaround far more tractable than "extract a whole game". **[V]**
6. SC-Docker's game files come from an unauthorized repack. That path should not be used, and
   should not be referenced from `bwapi-c2`. **[V]**

**Recommended next step:** install the free SKU under Wine/Lutris and `ls` for `*.mpq`. That is the
one question I could not answer remotely, and it decides everything downstream. See §7.

---

## 1. How SC-Docker works

Worth understanding even though we are not going to use its game files, because the architecture is
the reference design for headless BWAPI play and parts of it are reusable.

Five image layers, built bottom-up by `docker/build_images.sh`: **[V]**

| Image | Contents |
|---|---|
| `starcraft:wine` | `ubuntu:18.04`, WineHQ stable, `WINEARCH=win32`, Xvfb, x11vnc |
| `starcraft:bwapi` | Redistributable DLLs, `winetricks vcrun2015`, `bwheadless.exe`, cached BWAPI versions, SSCAIT tournament module |
| `starcraft:java` | 32-bit JRE for JNI/mirror bots |
| `starcraft:game` | The StarCraft files (built locally, never published — see §2) |
| `starcraft:play` | Orchestration entrypoint |

### Privilege separation

Two users are created. `starcraft` takes its UID from a build arg so bind-mounted volumes don't end
up root-owned. `bot` (UID 2001) gets its own Wine prefix and can only reach `bwapi-data/read`,
`write`, and `AI`. **[V]**

This is not incidental. Bot submissions are arbitrary native Windows binaries. Any local harness we
build should keep this property.

### Launch chain

`docker/scripts/launch_game`: **[V]**

```bash
winegui "$SC_DIR/bwheadless.exe" \
    -l "$BWAPI_DATA_DIR/BWAPI.dll" \
    -e "$SC_DIR/StarCraft.exe" \
    --installpath "$SC_DIR" "$@"
```

`bwheadless` injects `BWAPI.dll` into `StarCraft.exe` and suppresses rendering. `play_common.sh`
then generates `bwapi.ini` with `sed`.

### Networking — the part worth stealing

`docker/bwapi/bot/bwapi.ini`: **[V]**

```ini
auto_menu = LAN
lan_mode = Local Area Network (UDP)
```

There is no game server. Each player gets its own container running a full StarCraft process. They
discover each other over StarCraft's original LAN UDP protocol on a Docker bridge network at
`172.18.0.0/16` (`scbw/docker_utils.py:30`). BWAPI's `auto_menu` drives the menus so nothing has to
click. Headful mode is just Xvfb plus x11vnc on 5900+. **[V]**

N players means N full game processes. That is the cost of the injection model, and it is one of the
arguments for OpenBW below.

---

## 2. Where SC-Docker gets the game — and why we won't

`docker/build_images.sh:12`: **[V]**

```bash
[ ! -f starcraft.zip ] && curl -SL 'http://files.theabyss.ru/sc/starcraft.zip' -o starcraft.zip
```

`scbw/local_docker/game.dockerfile:8`: **[V]**

```dockerfile
# Get Starcraft game from ICCUP
COPY starcraft.zip /tmp/starcraft.zip
```

`README.md:74` lists it as a feature: *"StarCraft 1.16.1 game from ICCUP (no need for special
installs!)"* **[V]**

So: a repackaged portable StarCraft 1.16.1 originally from iCCup (a third-party ladder that
distributed its own bundle), re-hosted on a Russian community file host. The zip is a complete copy
of the game — `StarCraft.exe`, `StarDat.mpq`, `BrooDat.mpq`, `patch_rt.mpq` — with no CD key check
and no ownership verification.

**`files.theabyss.ru` no longer resolves.** **[V]** The canonical build path is broken, and the
BASIL fork (`basil-ladder/sc-docker`), which runs the modern ladder, carries the identical dead URL. **[V]**

### The fig leaf, and why it's thin

SC-Docker's `LICENSE` is MIT and covers only their scripts. **[V]** And I enumerated all **86 tags**
on Docker Hub's `ggaic/starcraft`: `wine`, `bwapi`, `java`, `play`, `replay-parser`,
`rabbitmq-broker`, plus version-pinned variants. **There is no `game` tag.** **[V]** Every layer
except the one holding Blizzard's bits is published. Someone thought about this.

But "we ship the script, not the payload" is a contributory-infringement posture, not a defense
against one. The `curl` is the single documented path to a working install, the project is
non-functional without it, and the README advertises the bundled game as a selling point. Whoever
hosted the zip was infringing; everyone who runs `build_images.sh` makes a copy.

**Decision: we do not use this path, and `bwapi-c2` does not reference it.**

---

## 3. Legal analysis

Not legal advice. But the shape is clear enough to make engineering decisions from.

### The version trap

Blizzard made StarCraft and Brood War free in 2017 with patch 1.18. **[S]** That does not rescue
BWAPI, for two reasons:

1. Free-to-download is not freely-redistributable. The EULA still governs.
2. **Patch 1.18 deliberately broke BWAPI.** BWAPI requires 1.16.1 — our own `README.md:41` says so. **[V]**

So the project needs binaries Blizzard never released for free and does not distribute at all
anymore. There is no way to launder this through the 2017 free release.

Our own repo already knows about the repack: `README.md:52` warns *"Make sure the version is set to
Starcraft 1.16.1, not ICCup 1.16.1"*. **[V]**

### The licensed 1.16.1 distribution (mostly dead)

SSCAIT's tutorial page says, verbatim: **[V — I read this off the live page]**

> Download and unzip StarCraft 1.16.1 from Memorial University, hosted with permission from
> Activision Blizzard. Newer versions of StarCraft like Remastered are incompatible with BWAPI.

That is Dave Churchill's server at Memorial University; he runs the AIIDE StarCraft AI Competition.
His STARTcraft repo links a bundle from the same host.

Caveats:
- **Both MUN URLs now 404.** `/starcraftaicomp/` returns 200, but `/files/` and `/files/startcraft/`
  are gone. The competition site moved to `davechurchill.ca/starcraft/`, and STARTcraft's README
  still points at the dead link. **[V]**
- Wayback has a snapshot of `Starcraft_1161.zip` dated 2025-08-23. **[V]**
- The permission claim is **secondhand** — SSCAIT asserting something about MUN's arrangement. I
  could not find Churchill's or Blizzard's own statement of it. **[?]** And Wayback re-serving the
  file is not covered by whatever permission MUN had.

Better provenance than theabyss.ru. Not a license you can point to.

### BWAPI itself vs. distributing the game

Worth separating, because the risk is very different:

- **BWAPI** injects a DLL and rewrites game memory. That plausibly trips the EULA's anti-modification
  clauses. It ships **no game files**; its entire `## Legal` section is a trademark acknowledgment. **[V]**
- **Distributing the game** is copyright with statutory damages.

EULA breach is a contract problem. Redistribution is not. Different orders of magnitude.

### MDY v. Blizzard — directly on point

*MDY Industries, LLC v. Blizzard Entertainment, Inc.*, 629 F.3d 928 (9th Cir. 2010) — the WoW Glider
bot case. Same defendant, same question shape. **[S]**

- Glider users were **not** copyright infringers. The court held the EULA/ToU terms were **covenants,
  not license conditions** — so breach is a contract claim, not copyright.
- But MDY **did** violate **§1201(a)(2)** as to WoW's *dynamic non-literal elements* (the live game
  experience), because Warden controlled access to those and Glider avoided it.
- Warden did **not** effectively control access to the **literal elements** (code already on your
  disk), so no §1201(a)(2) there — and no §1201(b)(1) liability at all.

**Applied:** offline/LAN on 1.16.1, there is no server-side dynamic experience being accessed and no
Warden bypass, so the §1201 hook doesn't attach. What remains is contract exposure. On 1.18/Remastered,
if you had to defeat an integrity check to inject, that is precisely the MDY §1201(a) fact pattern.

**Consequence:** "just redo the offsets for 1.18" would convert a contract problem into a DMCA problem.
That is an argument against ever porting the injection layer forward.

### Anti-cheat correction

Warden has been in StarCraft **since patch 1.15**. **[S]** So 1.16.1 has it too. BWAPI is not safe
because 1.16.1 lacks anti-cheat — it is safe because BWAPI runs offline/LAN, never authenticates to
Battle.net, and never bypasses Warden. Note that the current build manifest carries a
`ReleaseAntiCheat` tag. **[V]**

---

## 4. Why BWAPI cannot simply be re-offset for 1.18

The question was whether this is "just redoing binary analysis to find offsets." It is not. Evidence
from our tree, in ascending order of difficulty:

**1. Offsets — the easy part.** 208 hardcoded `0x00xxxxxx` addresses, 103 of them in
`bwapi/BWAPI/Source/BW/Offsets.h`. Mechanical. **[V]**

**2. In-place machine code rewriting.** `bwapi/BWAPI/Source/CodePatch.cpp`: **[V]**

```cpp
HackUtil::WriteNops(BW::BWFXN_SpendRepair, 7);
HackUtil::JmpPatch(BW::BWFXN_SpendRepair, &_repairHook);
...
HackUtil::WriteNops(BW::BWDATA::SingleSpeedHack, 11);
```

`WriteNops(addr, 7)` asserts *this instruction sequence is exactly 7 bytes*. That is not a
relocatable symbol; it is a bet on the compiled encoding. `DLLMain.cpp:275` spawns a
`PersistentPatch` thread that re-applies these continuously. **[V]**

**3. Mid-function entry and return.** 29 unique `BWFXN_*` targets, including
`BWFXN_RefundGasReturnAddress`, `BWFXN_RefundMin4ReturnAddress`. BWAPI jumps into the middle of
Blizzard's functions and returns to specific instructions. **[V]**

**4. Reverse-engineered struct layouts.** `CUnit`, `CBullet`, `CSprite`, `CImage`, `CThingy`,
`dialog`, `Font`, `Path` — byte layout implied by declaration order. A recompile can reorder or
repad any of them, and it fails *silently*. **[V]**

**5. The renderer is gone.** This is the structural blocker. BWAPI hooks DirectDraw throughout —
`PrimarySurface`, `BackSurface`, `GamePalette`, `DDInterface` in `Offsets.h`, plus Storm's
`SDrawLockSurface` / `SDrawUnlockSurface` / `SDrawCaptureScreen`. **[V]** Patch 1.18 replaced
DirectDraw with an OpenGL backend. **[S]** So `WMode.cpp`, `Graphics.cpp` and `Resolution.cpp` are
not offset-shifted — the API they hook no longer exists.

**6. Storm.dll hooked by ordinal.** `CodePatch.cpp:60-74` patches ordinals 119, 121, 128, 267, 268,
342, 350, 356, 401, 501. Ordinals are a build artifact. **[V]**

**7. The DMCA layer.** See §3.

Items 2–6 are a port, not a rescan. Item 7 says don't.

---

## 5. What Blizzard actually ships today

This is the section that changed the plan. All of it is reproducible — see Appendix A.

### Two products exist

Querying Blizzard's NGDP version endpoints: **[V]**

```
s1   → 1.23.10.13515   StarCraft (Remastered line)
s1a  → 1.19.0.2407     StarCraft Anthology (free classic line)
s2   → 5.0.16.97563    StarCraft II
```

`s1a` is a genuinely separate product code pinned at 1.19. That is the free classic SKU.

### The current client has no MPQs

I fetched `s1`'s build config from `level3.blizzard.com/tpr/sc1live/`, BLTE-decoded the install
manifest, and parsed it: **[V]**

```
decoded 100737 -> 105144 bytes
install manifest v1, hash_size=16, 27 tags, 1331 entries
total install size: 421,308,407 bytes

=== entries matching mpq ===
(none)
```

What it does contain:

```
15,734,912  x86_64/StarCraft.exe
53,723,112  x86_64/cef/libcef.dll          <- Chromium, for the launcher UI
16,766,592  x86_64/ClientSdk.dll
 5,004,152  StarCraft Launcher.exe

extensions: htm:640, pak:175, scm:163, scx:160, strings:42, png:36, dll:30, exe:11, ...
```

Executables, a Chromium embed for the launcher, and 323 campaign/melee maps (`.scm`/`.scx`). The
actual game assets live in CASC, fetched via the separate download manifest. Community reports agree
that Remastered dropped MPQ for CASC. **[S]**

Manifest tags, for reference: `NOLA`, 16 locales, `OSX`, `Windows`, `x86`, `x86_64`, `Debug`,
`Release`, `ReleaseAntiCheat`, `ReleaseAssert`, `igr`, `noigr`. **[V]**

### The gap I could not close

I tried to pull `s1a`'s build config (`e32f46c7245bfc154e43924555a5cf9f`) to see whether the free
Anthology SKU differs. It is not served from the live CDN — 403 on every `tpr/` path I probed
(`sc1a`, `sc1`, `sc1live`, `sc1adev`, `starcraft`, `sc1anthology`, `s1a`). **[V]**

So: **does the free SKU install the old MPQ-based 1.19 client, or the modern CASC client?** **[?]**
My manifest evidence points to CASC. The separate `s1a` product code at 1.19 leaves real room for the
other answer. This is the experiment in §7.

### Confirming the bootstrapper

```
http=200 size=4896464 type=application/x-msdos-program
url=https://downloader.battle.net/download/installer/win/1.0.66/Battle.net-Setup.exe
Battle.net-Setup.exe: PE32 executable (GUI) Intel 80386, for MS Windows, 5 sections
```

4.9 MB. No game data. **[V]**

---

## 6. What OpenBW actually requires

OpenBW is a from-scratch reimplementation of the Brood War engine. It needs no `StarCraft.exe`, no
Wine, no injection, no code patching. It runs natively on Linux and is debuggable under gdb. That
makes it the right target for protocol work regardless of the licensing question.

### It hard-requires MPQ containers

`openbw/data_loading.h:1319`: **[V]**

```cpp
data_files_loader_T data_files_directory(a_string path) {
    if (!path.empty() && path[path.size()-1] != '/' && path[path.size()-1] != '\\') path += '/';
    data_files_loader_T r;
    r.add_mpq_file(path + "Patch_rt.mpq");
    r.add_mpq_file(path + "BrooDat.mpq");
    r.add_mpq_file(path + "StarDat.mpq");
    return r;
}
```

`data_files_loader` (line 1300) exposes only `add_mpq_file` (line 1303). There is no loose-file or
directory backend. OpenBW ships its own MPQ reader — it parses the `0x1a51504d` signature itself at
line 1173, with its own crypt table and decompression. No StormLib dependency. **[V]**

### But the seam is small

The loader is a template whose entire consumer-facing interface is one method: **[V]**

```cpp
void operator()(a_vector<uint8_t>& dst, a_string filename)
```

A directory-backed replacement is on the order of 15 lines. This is the hook for the CASC workaround.

### The headless data surface is tiny

Files the headless engine reads from the MPQs: **[V]**

```
arr/units.dat      arr/weapons.dat    arr/flingy.dat     arr/sprites.dat
arr/images.dat     arr/images.tbl     arr/orders.dat     arr/techdata.dat
arr/upgrades.dat   scripts/iscript.bin
```

The UI layer additionally wants `arr/sfxdata.dat`, `arr/sfxdata.tbl`, `game/*.pcx`, and the `unit/`
and `sound/` trees. **[V]**

**This is the most useful finding for the workaround.** Headless play needs about ten small tables,
not the hundreds of megabytes of art and audio. Extracting those from CASC is a far smaller job than
"reconstruct the game".

### Version caveat

OpenBW's BWAPI fork sits at BWAPI 4.2.0. **[S]** bmnielsen's `StardustDevEnvironment` notes that the
BWAPI 4.4 latency-compensation changes had to be backported. **[S]** For protocol work this is
friction, but it is also arguably the right place to be doing the work.

---

## 7. Options, ranked

### A. Install the free SKU under Wine and look — **do this first**

Lutris has a Battle.net install script. Install the free StarCraft Anthology SKU, then:

```bash
find ~/Games -iname '*.mpq' -o -iname 'StarDat*' -o -iname 'BrooDat*' 2>/dev/null
```

If MPQs land on disk, we are done — point OpenBW at that directory and move on.

My manifest evidence says they won't. But the separate `s1a` product code at 1.19 is real, and this
is a 30-minute experiment that closes the only open question. Everything else is contingent on it.

**Cost:** ~30 min. **Information value:** decides the whole plan.

### B. Extract from CASC

CascLib (Ladislav Zezula — same author as StormLib) builds on Linux. Pull the classic data tree out
of your own licensed install, then either:

- repack into MPQs with StormLib/MPQEditor, or
- write the ~15-line directory-backed loader for OpenBW (§6) and skip repacking entirely.

The second is cleaner and avoids a lossy round-trip through a container format we don't need.

Legally clean: your own copy, your own machine, no redistribution.

**Unverified:** whether Remastered's CASC exposes the classic tree under the `arr/*.dat` names
OpenBW expects. **[?]** Given the headless list is ten files, the spike is cheap.

### C. Original media

`StarDat.mpq` is on the StarCraft disc, `BrooDat.mpq` on the Brood War disc. `patch_rt.mpq` comes
from a patch, and the Internet Archive hosts a Blizzard-official "StarCraft Patches" collection.

Best provenance available. Contingent on having the discs.

Note: I did not verify the final retail CD patch level. It matters less than expected, because
OpenBW wants the data files, not a specific executable version.

### D. The MUN bundle via Wayback

The AIIDE-blessed 1.16.1 zip, snapshot 2025-08-23. Decent provenance, dead live link, and Wayback
re-serving is not covered by whatever permission MUN had.

Fallback only. **Do not reference from `bwapi-c2`.**

### Not an option

SC-Docker's `starcraft.zip`. Dead host, unauthorized repack, and referencing it from our own project
would put BWAPI in the contributory-infringement posture described in §2.

---

## 8. Open questions

| # | Question | How to close |
|---|---|---|
| 1 | Does the free Anthology SKU install MPQs or CASC? | §7A — install under Lutris, `find -iname '*.mpq'` |
| 2 | Does Remastered's CASC expose `arr/*.dat` under recognizable names? | CascLib spike against a real install |
| 3 | Is the MUN "permission from Activision Blizzard" claim documented anywhere first-party? | Ask Dave Churchill directly |
| 4 | Does `bwapi-c2` sit above the BWAPI client API, or does it reach into the injection layer? | Design decision — drives whether OpenBW is sufficient |

Question 4 is the architectural one. If `bwapi-c2` targets the client-side protocol (the
`client-bridge-*.dll` shared-memory surface), OpenBW is a drop-in and should be the primary dev
target. If it reaches into the injection layer, that is a much larger scope question and deserves
its own ADR — informed by §3's conclusion that porting injection to 1.18 has a DMCA dimension.

---

## Appendix A: Reproducing the CDN investigation

No credentials required; all endpoints are public.

```bash
# 1. Product versions
curl -sS https://us.patch.battle.net/s1/versions     # StarCraft (Remastered line)
curl -sS https://us.patch.battle.net/s1a/versions    # StarCraft Anthology (free classic)

# 2. CDN hosts and path
curl -sS https://us.patch.battle.net/s1/cdns
#   -> Path = tpr/sc1live, Hosts = level3.blizzard.com us.cdn.blizzard.com

# 3. Build config (hash from step 1, column BuildConfig)
H=864772b9ff94f6d372aa4ee90ee2f8ab
curl -sS "http://level3.blizzard.com/tpr/sc1live/config/${H:0:2}/${H:2:2}/$H"
#   -> install = <CKey> <EKey>

# 4. Install manifest, fetched by EKey (second hash on the install line)
E=311e6a69ea379077313e68b06d9126ed
curl -sS -o install.blte "http://level3.blizzard.com/tpr/sc1live/data/${E:0:2}/${E:2:2}/$E"

# 5. Decode and list
python3 docs/tools/blte_install_manifest.py install.blte
```

Hashes change as Blizzard patches. Re-run steps 1 and 3 to get current ones.

### Confirming the bootstrapper

```bash
curl -sSL -o Battle.net-Setup.exe \
  "https://www.battle.net/download/getInstallerForGame?os=win&gameProgram=BATTLENET_APP&version=LIVE"
ls -l Battle.net-Setup.exe && file Battle.net-Setup.exe
```

---

## Appendix B: BLTE decoder

Saved at `docs/tools/blte_install_manifest.py`. Decodes Blizzard's BLTE container format and parses
the TACT `IN` install manifest.

Supports BLTE modes `N` (raw), `Z` (zlib), and `F` (recursive frame). Mode `E` (Salsa20-encrypted)
is not supported — it would need TACT keys, and the install manifest does not use it.

Usage:

```bash
python3 docs/tools/blte_install_manifest.py install.blte              # summary + mpq search
python3 docs/tools/blte_install_manifest.py install.blte --all        # every entry
python3 docs/tools/blte_install_manifest.py install.blte --grep .dat  # filter
```

---

## Appendix C: Source references

### In this repository
- `README.md:41` — BWAPI requires StarCraft 1.16.1
- `README.md:52` — distinguishes Blizzard 1.16.1 from the iCCup repack
- `README.md:102` — `## Legal` section (trademark acknowledgment only)
- `bwapi/BWAPI/Source/BW/Offsets.h` — 103 hardcoded addresses, 29 `BWFXN_*` targets, DirectDraw types
- `bwapi/BWAPI/Source/CodePatch.cpp:29` — `WriteNops` / `JmpPatch` / `CallPatch`
- `bwapi/BWAPI/Source/CodePatch.cpp:60-74` — Storm.dll ordinal hooks
- `bwapi/BWAPI/Source/DLLMain.cpp:275` — `PersistentPatch` thread

### SC-Docker
- `docker/build_images.sh:12` — the `theabyss.ru` fetch
- `scbw/local_docker/game.dockerfile:8` — "Get Starcraft game from ICCUP"
- `docker/bwapi/bot/bwapi.ini` — `auto_menu = LAN`, `lan_mode = Local Area Network (UDP)`
- `scbw/docker_utils.py:30` — `SUBNET_CIDR = "172.18.0.0/16"`
- `LICENSE` — MIT, covers scripts only

### OpenBW
- `data_loading.h:1173` — own MPQ signature parser
- `data_loading.h:1300,1303` — `data_files_loader` / `add_mpq_file`
- `data_loading.h:1319-1324` — `data_files_directory`, the three hardcoded MPQ names

### External
- SSCAIT tutorial — https://sscaitournament.com/index.php?action=tutorial
- STARTcraft — https://github.com/davechurchill/STARTcraft
- OpenBW BWAPI fork — https://github.com/OpenBW/bwapi
- StardustDevEnvironment — https://github.com/bmnielsen/StardustDevEnvironment
- SC-Docker — https://github.com/Games-and-Simulations/sc-docker
- SC-Docker technical report — arXiv:1801.02193
- MDY v. Blizzard, 629 F.3d 928 (9th Cir. 2010) — https://caselaw.findlaw.com/court/us-9th-circuit/1548042.html
- Patch 1.18 — https://liquipedia.net/starcraft/Patch_1.18
- Warden — https://wowpedia.fandom.com/wiki/Warden_(software)
