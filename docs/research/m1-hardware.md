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
_Pending: first run on the user's PSP-3000._

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
_Pending._
