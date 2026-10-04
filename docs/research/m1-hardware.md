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

**Next:** repeat with a retail ISO (expected: P2 = 24 MB, P9 present with Inferno's 8 MB cache).

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
