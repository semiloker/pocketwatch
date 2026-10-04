# M1 — hardware ground truth (PSP-3000, ARK-4 6.61)

Goal: replace assumptions with measurements before any savestate engine code is written.

## S1 — memory map with a game running

Build: debug (`DEBUG=1`, default). The plugin writes `ms0:/seplugins/pocketwatch/pocketwatch.log`.

Procedure:
1. `scripts\build.cmd` then `scripts\deploy.cmd -Enable`, eject the PSP from Windows.
2. Start a game (ISO), get into actual gameplay.
3. Hold **L + R + SELECT + UP** for about a second: one full dump per press.
4. Exit the game, reconnect USB, read the log.

The log contains:
- boot snapshot: model, devkit version, apitype, game ID (`disc0:/UMD_DATA.BIN`), init file,
  eDRAM size, every memory partition (start, size, free, largest free block);
- import self-check (every import resolved by ARK-4?);
- per dump: the same plus all modules (text/data/bss, first segment) and all threads
  (attributes: user/kernel, priority, status, wait type, stack).

Questions it answers:
- Where P2/P5/P8/P9/P11 really are on this unit, and how much of P9 is left after Inferno's cache.
- Which modules live in kernel RAM (what a kernel snapshot must contain).
- Which threads belong to the game (to freeze) vs. the system/ARK (to keep running).
- eDRAM size (2 MB or 4 MB) for the VRAM part of a state.

### Results

**Run 1 — 2026-10-04, homebrew (raylib test, apitype 0x141), not a retail game.**
Raw log: [`logs/2026-10-04-s1-homebrew-raylib.log`](logs/2026-10-04-s1-homebrew-raylib.log).
The plugin was registered as `GAME`, which in ARK-4 also matches homebrew; it is now `UMD`.

- Unit: `sceKernelGetModel() = 3` (PSP-3000), devkit `0x06060010`, eDRAM **2048 KB**.
- Import self-check: 28 imports, **0 unresolved** — every allowlisted NID is now `verified`.
- The homebrew requested large memory, so this is the *highmem* layout, not the game one:

  | Partition | Start | Size | Free at boot | Free in app |
  |---|---|---|---|---|
  | 1 (kernel) | 0x88000000 | 3072 KB | 270 KB | 307 KB |
  | 2 (user) | 0x08800000 | 52224 KB | 51033 KB | 764 KB |
  | 4 (kernel) | 0x88300000 | 1024 KB | 1024 KB | 1024 KB |
  | 5 (volatile) | 0x08400000 | 4096 KB | 4096 KB | 4096 KB |
  | 8 | 0x8BB00000 | 1024 KB | 681 KB | 681 KB |
  | 9 | 0x8BB00000 | **0 KB** | 0 | 0 |
  | 11 | 0x8BC00000 | 4096 KB | 4096 KB | 4096 KB |

  (3 mirrors 1, 6 mirrors 2, 12 mirrors 8.)
- **Kernel RAM is nearly full: ~270 KB free in P1.** Other plugins are big (RemoteJoyLite alone has
  263 KB of bss). Pocketwatch must keep its kernel footprint small and put every large buffer
  elsewhere (P11 = 4 MB free here; P9 in retail games, to be measured).
- Threads: the app's threads have attribute `0x80000000` (user); system and plugin threads are
  kernel threads. Freezing "user threads only" is a clean criterion.
- 57 modules; all kernel modules sit in 0x88000000–0x8828FFFF; the app's module is in P2.

**Incident:** exiting the homebrew hung on "Please wait..." (needed a forced power-off).
Cause: the probe thread kept running after `module_stop`, inside a module being unloaded.
Fix: `module_stop` now signals and joins every plugin thread (terminate after 500 ms), dumps are
abortable between lines, `disc0:` is only touched for retail apitypes, and the plugin loads for
retail games only.

**Run 2 — 2026-10-04, retail ISO: Need for Speed Most Wanted 5-1-0 (ULUS-10036, apitype 0x123).**
Raw log: [`logs/2026-10-04-s1-nfs-most-wanted.log`](logs/2026-10-04-s1-nfs-most-wanted.log).
Two dumps in gameplay (identical). Exit back to the XMB with the fixed build: no hang reported.

| Partition | Start | Size | Free at boot | Free in game | Role |
|---|---|---|---|---|---|
| 1 (+3 mirror) | 0x88000000 | 3072 KB | 423 KB | **319 KB** | kernel |
| 4 | 0x88300000 | 1024 KB | 1024 KB | 875 KB | kernel (game-loaded drivers) |
| 2 (+6 mirror) | 0x08800000 | 24576 KB | 20303 KB | **517 KB** | game |
| 5 | 0x08400000 | 4096 KB | 4096 KB | 4096 KB | volatile (unused by this game) |
| 8 (+12 mirror) | 0x8BB00000 | 1024 KB | 681 KB | 681 KB | |
| **9** | **0x8A000000** | **24576 KB** | 24576 KB | **24576 KB** | **entirely free** |
| 11 | 0x8BC00000 | 4096 KB | 4096 KB | 4096 KB | free |

Findings that shape the engine:
- **28 MB of free RAM during gameplay (P9 + P11).** Inferno's ISO cache did not take P9 in this
  game. A complete state (4 MB kernel + 24 MB user + 2 MB VRAM, compressed) can be staged in RAM,
  so the slow Memory Stick write can happen outside the frozen/suspended window.
- **The game uses ~98% of P2** (517 KB free): "save only allocated blocks" would gain nothing; save
  all 24 MB and let compression handle the rest.
- **Kernel = 0x88000000..0x88400000 (P1 + P4, 4 MB)**: this is the kernel snapshot range. The game
  loaded 7 drivers into kernel RAM at runtime (sceSAScore, sceMpegbase_Driver, sceNet_Service,
  sceVideocodec_Driver, ...) and 9 user libraries into the top of P2 (sceNet*, sceMpeg,
  ATRAC3plus) — all covered by "kernel 4 MB + P2 24 MB".
- **eDRAM = 2048 KB** on this PSP-3000 (not 4 MB).
- **Game threads** (`user_main`, `main`, `DVDUMD_READ`, `SND`, `SND_SAS`, `SND ATRAC PLAYER`)
  all have attr bit `0x80000000`. `SceIofileAsync` (attr `0x08100000`) is a kernel worker for the
  game's async I/O: the freeze must wait until it is idle rather than suspend it.
- Kernel P1 free: 319 KB. Pocketwatch occupies 23 KB there today; everything large goes to P9/P11.
- Plugins sharing kernel RAM: HUD, bright, battery_icon, Hold, CWCheat, RemoteJoyLite (263 KB bss),
  noumd.

**Still open:** a game that uses the volatile partition (P5) and a game where Inferno's cache does
claim P9 (it is `infernocache:lru` in SETTINGS.TXT), to size the worst case.

## S2 — are hash-less saves listed by the XMB?

`python tools/spikes/s2_savedata_test.py D:/PSP/SAVEDATA` creates:

| Folder | SAVEDATA_PARAMS | SAVEDATA_FILE_LIST | Accent colour |
|---|---|---|---|
| `PWTEST00A` | mode 0x00, all zero | empty | blue ring |
| `PWTEST00B` | mode 0x00, all zero | `STATE.BIN` with zero hash | gold ring |

Check on the PSP: **Game → Saved Data Utility**. For each folder note: listed or not, icon,
title "Pocketwatch test" / "Save State test A|B", detail text, background picture, any
"corrupted data" marking, and whether Delete works. Remove leftovers with `--remove`.

`tools/sfo.py` rebuilds all 21 real PARAM.SFO files found on the test PSP byte-for-byte, so the
generated files have exactly the layout of real saves.

### Results

**2026-10-04, PSP-3000:** neither folder appeared as "Pocketwatch test"; the Saved Data Utility
showed "Corrupted Data" entries instead (both folders were still on the card afterwards).
Conclusion: **mode 0x00 / hash-less saves are not accepted by the XMB.** States must be written
with valid SFO hashes (mode 0x01, as ARK_01234 does) — generated on the PSP itself, since the
detail text (date, slot) changes per save. Next spike: hash a PARAM.SFO on-device (sceChnnlsv or
the KIRK engine) and confirm the XMB lists it. Test folders removed with `--remove`;
to confirm, the "Corrupted Data" entries should now be gone.

## S2b — hashed saves written on the PSP

Debug build, in a game hold **L + R + SELECT + LEFT** (`plugin/src/debug/spike_savedata.c`):
1. Loads `flash0:/kd/chnnlsv.prx` if needed and resolves `sceSdSetIndex` / `sceSdRemoveValue` /
   `sceSdGetLastIndex` through ARK's `sctrlHENFindFunction` — the firmware does the crypto,
   Pocketwatch holds no keys.
2. Re-hashes `ARK_01234/PARAM.SFO` (an SFO the XMB accepts) and logs MATCH/MISMATCH against its
   stored hashes: an on-device check of the algorithm (PPSSPP `UpdateHash`, mode 0x01:
   MAC mode 2 -> params+0x20, set flag, MAC mode 1 -> params+0x10).
3. Writes `PWTEST00C` (empty file list) and `PWTEST00D` (`STATE.BIN` listed) with hashed SFOs and
   ICON0/PIC1 from the plugin's own PNG encoder, logging the time of each step.

Then check **Game -> Saved Data Utility** for "Pocketwatch test" C and D.

### Results

**2026-10-04, PSP-3000, in NFS Most Wanted.** Raw log:
[`logs/2026-10-04-s2b-nfs-most-wanted.log`](logs/2026-10-04-s2b-nfs-most-wanted.log).

- **The XMB lists PWTEST00C and PWTEST00D with icon and files** (user report). Saves written
  entirely by the plugin are accepted: the native Saved Data Utility can be the state browser.
- `chnnlsv.prx` loaded on demand as module `sceChnnlsv`; functions resolved via ARK. The 398 KB
  work buffer came from P9 at `0x8A000000`. Import self-check: 41 imports, 0 unresolved.
- Timings (game running, 222/333 MHz as set by the game):

  | Step | Time |
  |---|---|
  | build + hash PARAM.SFO | 2 ms |
  | encode ICON0 (34.7 KB) / write | 9 ms / 23-27 ms |
  | encode PIC1 (392 KB) / write | 95 ms / 129-213 ms (about 2-3 MB/s) |

- Both PNGs per folder verified on the PC with zlib (CRCs, inflate, dimensions).
- A second press re-ran the spike: `sceIoMkdir` returned `0x80010011` (already exists) and the
  files were overwritten — fine.
- **Open question:** re-hashing `ARK_01234/PARAM.SFO` gave **MISMATCH** with its stored hashes,
  while our own (firmware-computed) hashes are accepted. Either ARK's file was hashed differently
  and the XMB does not check the MAC values when listing, or the XMB checks only part of the
  protection data. Test **PWTEST00E** (made on the PC from C's SFO with a changed title and
  directory, i.e. a stale hash) decides it: listed normally = the XMB does not verify the hash.
  Either way Pocketwatch keeps computing real hashes (2 ms, correct by construction).
- **Answer (same day): PWTEST00E is not listed as "Save State test E" — it shows as
  "Corrupted Data". The XMB verifies the SFO MAC.** Therefore our chnnlsv hashes are valid, the
  hashing is mandatory, and ARK_01234's stored hash is most likely invalid itself (its entry would
  then be one of the "Corrupted Data" items the user saw). The ARK re-hash comparison is not a
  usable oracle; a folder written by the plugin and listed by the XMB is.

## M1 conclusions

1. States will be native save folders: `PW<GAMEID><NN>` with a chnnlsv-hashed (mode 0x01)
   PARAM.SFO, ICON0, PIC1 and the state data. Browsing and deleting come from the XMB for free.
2. In a retail game on this PSP-3000 there are 28 MB of free RAM (P9 24 MB + P11 4 MB) to stage a
   state; kernel RAM (P1) has ~300 KB free and must stay untouched by big buffers.
3. Snapshot ranges: kernel `0x88000000-0x88400000` (4 MB), game `0x08800000-0x0A000000` (24 MB),
   eDRAM 2 MB. Game threads are the `0x80000000`-attribute ones.
4. The Memory Stick writes at roughly 2-3 MB/s in a game: a compressed state of ~10-15 MB takes
   several seconds, so it must be written after the game is resumed or behind a native
   "Saving..." screen.
