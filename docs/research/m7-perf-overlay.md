# M7 — a performance overlay over Home and in games

Question: how can Pocketwatch show the PSP's numbers (frame rate, CPU, memory, battery) over Home
and over a running game, without costing the game what the overlay measures (owner, 2026-10-20)?
Status: Open (2026-10-20) - implemented (ADR-043); device tests HM31, PF1, PF2 pending.

## Background and sources

**The game plugin's budget.** 194 552 of 196 608 bytes of kernel RAM in the debug build, the
release budget 180 224 (`make check`, ADR-037, RISK-005): ~2 KB left - an overlay with its font,
its measurements and its drawing does not fit. **VERIFIED** (`make check` 2026-10-20).

**A module of its own.** ARK-4 loads every enabled line of `seplugins/PLUGINS.TXT` with the game
(`UMD, <path>, on`, the format `scripts/deploy.ps1` writes for the game plugin). **VERIFIED** (the
owner's PLUGINS.TXT, deploy log 2026-10-20). A second kernel module costs only while its line is
on; turning it off removes it from the next game.

**Finding the frame.** `sceDisplayGetFrameBuf` gives the frame the display shows; the hub draws
its own frame at display priority 0 (`sceDisplaySetFrameBufferInternal(0, ...)`,
`plugin/src/ui/fb.c`), which `sceDisplayGetFrameBufferInternal(0, ...)` returns while the hub is
open. **VERIFIED** for the hub's side (the code); the getter's NID is in ARK-4's table (0x5B5AEFAD
→ 0xF338AAE0, `tools/nids/ark4_660.tsv`). At the start of the vertical blank the next frame is
latched and the top lines are not yet scanned out, so lines written then show without tearing at
the top. **INFERRED** (the PSP's 272 visible of 286 lines; the write takes ~0.1 ms).

**The frame rate.** A new frame buffer address at a vblank is a frame the game showed (double and
triple buffering flip the address); a game that draws into one buffer only shows none ("--").
**INFERRED** from how `sceDisplaySetFrameBuf` is used by games.

**The CPU's load.** The PSP has one CPU; the idle threads (`idle0`, `idle1`) run when nothing else
does, so their run time over a second is the CPU left over. **VERIFIED** (the threads are listed
with those names, PPSSPP `docs/VSHBootInvestigation.md`; the hub's `cpuwatch` reads run times the
same way, `plugin/src/ui/cpuwatch.c`).

**Frozen or not.** The hub freezes only user-mode threads; kernel threads keep running
(`plugin/src/core/freeze.h`). **VERIFIED** (the code). So the overlay goes on over the hub.

**States.** A save captures the kernel image 0x88000000-0x88400000 and a load puts it back
(STE-REQ-001): the module and its thread are inside it. **INFERRED**: a load made without the
module removes it until the next game, a load made with it brings it back. RESEARCH REQUIRED: PF2.

**Drawing.** No GE (the game owns it): the CPU writes white pixels and a black outline into the
frame through its uncached address; nothing is read back (uncached VRAM reads are slow), which is
why there is no translucent card in games. The outline's search over the font is done once a second
into a bit mask; a vblank only writes the mask's pixels. **VERIFIED** (host test
`perf_overlay_draws_*`).

## Options

| | Where | Cost | Chosen |
|---|---|---|---|
| A | Inside the game plugin | ~4-6 KB over a 2 KB margin | no |
| B | A module of its own, always loaded | its RAM in every game | no |
| C | A module of its own, its PLUGINS.TXT line turned on by Home | its RAM only while on | yes (ADR-043) |

## Procedure

**PF1 - the overlay in games.** Home → Settings → Pocketwatch → Overlay in Games → Full; start a
game. Top left: "60 FPS  CPU 43%  333/166 MHz  RAM ... KB free" and "BAT 83%  -224 mA  4.10 V
20 C  2:31 left" (numbers as the PSP has them). Hold L + R + SELECT a second: FPS only; again: off;
again: Full. Open the hub (HOME): the lines stay over it. Note the game's frame rate with the overlay
on and off (FPS on its own costs least). Set Off in Home: the next game has none.

**PF2 - with states.** The overlay on: Save State, play, Load State, Quit Game; then the overlay
off and Load State of the same slot. Both loads work; the game runs on; Quit Game reaches Home.

## Results

None yet.

## Conclusions

- Over Home: Home's own frame numbers, its fonts, a card (the GE draws Home).
- In games: a separate kernel module (9.7 KB loaded), off by default, a combo to cycle it.
- OPEN QUESTION: the GE's busy time inside a game (no counter read yet).
- Next: HM31, PF1, PF2.
