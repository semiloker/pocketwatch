# Performance overlay — specification

| | |
|---|---|
| Area code | `PRF` |
| Status | Implemented 2026-10-20 (ADR-043): over Home, and in games as a module of its own; host-tested, checked in PPSSPP (Home only); device tests PF1, PF2, HM31 pending |
| Milestones | M7 |
| Code | [`perf/src/main.c`](../../perf/src/main.c) (the game module), [`perf/src/overlay.c`](../../perf/src/overlay.c) (its lines and drawing, portable), [`home/src/main.c`](../../home/src/main.c) (`perf_frame`: Home's numbers), [`home/src/home_view.c`](../../home/src/home_view.c) (`perf_overlay`), [`home/src/hsettings.c`](../../home/src/hsettings.c) (the two settings), [`home/src/plugins_txt.c`](../../home/src/plugins_txt.c) (ARK-4's plugin list) |
| Research | [m7-perf-overlay](../research/m7-perf-overlay.md) |

## A. Overview

- **Purpose:** show what the PSP is doing while a game or Home runs - the frame rate, the CPU's
  load and clock, the memory left, the battery (owner, 2026-10-20: "a performance overlay over
  the Pocketwatch menu and in games, to understand it - all the PSP's data").
- **Scope:** Home's overlay (its own frame loop's numbers); the game overlay (a kernel module ARK-4
  loads with the game when it is turned on; it also writes over the hub, which draws its own
  frame while the game is frozen).
- **Non-goals:** GE (graphics) load inside a game (the GE's own counters are not read: RESEARCH
  REQUIRED); logging the numbers to the card in games (nothing is written in a game by it); the
  overlay in the XMB (the XMB plugin is at its 64 KB budget, ADR-021).
- **Current state:** both overlays built; Home's checked in PPSSPP (59 fps, its CPU and GE time
  per frame, the clocks, memory, battery); the game module passes `make check` (every import
  ARK-mapped or allowlisted), 9.7 KB loaded. Nothing measured on the PSP yet.
- **Target state:** PF1/PF2 passed; the GE's busy time in games if a counter is found.

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `PRF-REQ-001` | Home → Settings → Pocketwatch → Performance Overlay (on / off, `home.txt perf`) shows over every Home screen, once a second: frames shown, Home's CPU and GE time a frame, its longest frame, the CPU / bus clock, the memory free outside Home's heap and the heap in use, the battery's percent, current, voltage and temperature - values the system does not give left out. | P2 | Implemented 2026-10-20; PPSSPP checked | PPSSPP, HM31 |
| `PRF-REQ-002` | Overlay in Games (Off / FPS / Full) turns the line `UMD, ms0:/seplugins/pocketwatch/pocketwatch_perf.prx, on|off` of ARK-4's PLUGINS.TXT on or off (adding it once) and writes `perf.txt` (PRF-REQ-008); every other line of PLUGINS.TXT is kept byte for byte (CRLF too); it takes effect at the next game start (a toast says so). | P2 | Implemented 2026-10-20; PPSSPP checked (the file written, off again) | `plugins_txt_*`, PF1 |
| `PRF-REQ-003` | In a game the module writes, at every vblank, its lines into the frame about to be shown: the hub's frame (display priority 0) while the hub is open, else the game's. FPS: frames the game showed in the last second (a new frame buffer at a vblank); Full adds the CPU's busy share (100 less the idle threads' run time), the clocks, the user memory free, the battery's percent, current (every 10 s), voltage, temperature and the system's time left. Unknown values are left out. | P2 | Implemented 2026-10-20 | `perf_overlay_*`, PF1 |
| `PRF-REQ-004` | L + R + SELECT held a second (no other button) switches the game overlay Off → FPS → Full → Off for the rest of that game. | P2 | Implemented 2026-10-20 | PF1 |
| `PRF-REQ-005` | The game module costs the game nothing it does not ask for: not loaded at all while it is off; when on, < 16 KB of kernel RAM (`make check`), no card writes, no hooks into the game (it reads the display and the controller like the hub), its drawing < 1 % of the CPU (the line's mask is made once a second; a vblank only writes its pixels). | P1 | Implemented; 9.7 KB (`make check` 2026-10-20); the CPU share RESEARCH REQUIRED (PF1) | `make check`, PF1 |
| `PRF-REQ-006` | With Volume and Brightness at Home and Games, the module shows in a game, for 1.5 s after a change of the volume, mute or the SCREEN button's step (read every 6th vblank), a black box with a white edge at the bottom middle: "VOL 18" / "MUTE" / "BRIGHT 3/4 92%" and a bar of 30 or 4 steps (ADR-045). | P1 | Implemented 2026-10-20; host tests `indicator_*` | tests, PF3 |
| `PRF-REQ-007` | With Brightness Boost on, the module keeps the backlight at the system's level plus the boost's share of the way to 100 whenever the system changes it; the screen off stays off; another setting its own back 4 times within 400 ms of ours makes it give up (ADR-045). | P2 | Implemented 2026-10-20; host test `brightness_*` | tests, PF3 |
| `PRF-REQ-008` | `perf.txt`: `mode = off|fps|full`, `osd = off|home|all`, `boost = 0-100` (missing: full, home, 0); the module's PLUGINS.TXT line is on while a game needs it (the numbers, the indicator in games, a boost) and off otherwise. | P1 | Implemented 2026-10-20; host test `perf_cfg_*`; PPSSPP checked (both files written) | tests, PF3 |
| `PRF-NFR-001` | A game with the overlay on saves and loads states as without it (RISK-027). | P1 | RESEARCH REQUIRED | PF2 |

## C. Architecture and integration

- **Home:** `perf_frame` (main.c) adds each frame's CPU time (`t_cpu`) and the GE's wait
  (`hm_gu_end`) and, once a second, formats two lines into `hm_view.perf`; `hm_draw` draws them
  after the style's screen, before the fade (a translucent card, Home's body font). The battery's
  numbers are the battery thread's (hw.c, never read in the frame loop: ADR-031).
- **Games:** `pocketwatch_perf.prx` (perf/), a kernel module ARK-4 loads from PLUGINS.TXT at the
  game's start, beside the game plugin (which knows nothing of it). One kernel thread at priority
  0x10: `sceDisplayWaitVblankStart`, the buttons (`sceCtrl_driver` peek), the frame
  (`sceDisplayGetFrameBufferInternal(0)` - the hub's - else `sceDisplayGetFrameBuf`), its uncached
  address, the masks written (`pw_perf_draw`). Once a second it measures and renders the masks
  (`pw_perf_render`: an 8x8 font - the game plugin's, MIT - with a black outline, so no VRAM is
  read). Kernel threads are not frozen with the game (core/freeze.h), so it goes on while the hub is
  open.
- **Files:** `seplugins/PLUGINS.TXT` (ARK-4's; only this module's line is changed), `seplugins/
  pocketwatch/perf.txt` (read once by the module), `home.txt` (`perf = on|off`).
- **Failure handling:** the module missing from the card: ARK-4 does not load it (INFERRED);
  a frame narrower than 480 px or none: nothing drawn; a value the system refuses: left out.

## D. User experience and visual design

Top left, two lines at most. Home: Home's body font on a translucent black card. Games: the 8x8
font, white with a black outline, directly on the picture (no card: a card would need the
picture's pixels read back). FPS mode: "60 FPS" only.

## E. Reference material

| Reference | Source | What to learn | Adopt / adapt / avoid | License |
|---|---|---|---|---|
| The hub's frame and the freeze | `plugin/src/ui/fb.c`, `plugin/src/core/freeze.h` | display priority 0 for the hub; kernel threads keep running | adopt | ours |
| Thread run times | `plugin/src/ui/cpuwatch.c` | `SceKernelThreadInfo.runClocks`, the idle threads | adopt | ours |
| 8x8 font | `plugin/src/ui/font8x8.c` (PSPFatSave) | the glyphs | adopt (linked, not copied) | MIT |

## F. Implementation plan

1. ~~Home's overlay, the settings, PLUGINS.TXT, the game module~~ (2026-10-20).
2. PF1, PF2 on the device; the CPU share of the module measured (`cpuwatch` in the hub's log).
3. The GE's busy time in games (RESEARCH REQUIRED: a GE counter or the list's end interrupt).

## G. Testing and acceptance

| Requirement | Test | Acceptance criterion | Needs |
|---|---|---|---|
| PRF-REQ-001 | HM31 | the lines change every second, nothing unknown shown as a number | PSP |
| PRF-REQ-002..004 | PF1 | the overlay in the next game; the combo cycles it; Off removes it at the next game | PSP |
| PRF-REQ-005, PRF-NFR-001 | PF1, PF2 | the game's frame rate the same with FPS on and off; save, load, Quit Game as without it | PSP |
| PRF-REQ-006..008 | PF3 | the indicator on each press; the boost brighter, the screen-off and SCREEN still work; the line off again with everything off | PSP |

## H. Risks and decisions

[ADR-043](../architecture/decisions.md#adr-043--a-performance-overlay-over-home-and-in-games-as-a-module-of-its-own);
[RISK-027](../risk-register.md#risk-027--the-overlay-modules-thread-runs-through-a-save-or-a-load) (a
thread of another module through the states engine's save and load);
[RISK-005](../risk-register.md#risk-005--kernel-ram-and-prx-size-growth) (P1 shared with the game's drivers).
