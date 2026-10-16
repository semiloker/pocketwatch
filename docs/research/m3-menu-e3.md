# Minimal in-game menu + E3 (states on the Memory Stick) — design

Research of 2026-10-16 (prior art: PSPFatSave, PSPStates v2, Custom Home, psplink, tempar,
MacroFire, ARK-4, Adrenaline; uOFW). Two phases, each tested on the device before the next.

## Phase 1 — minimal menu (drives the RAM states first)

Status: built 2026-10-16 (src/ui: draw, font8x8 from PSPFatSave/MIT, input, fb, menu; engine
`pw_engine_save_held`/`pw_engine_load_held`), awaiting the device test.

A plain but solid overlay now; the native XMB-style hub (M3/M4) replaces only the drawing later.

**Display: our own framebuffer, not the game's.** PSPFatSave draws into the game's on-screen
buffer (four pixel formats, re-pinning for double-buffered games, POPS residue) and, worst for us,
a state saved while the menu is up would carry the menu in its VRAM. Custom Home, psplink, tempar
and MacroFire instead show their own buffer with `sceDisplaySetFrameBufferInternal` at a higher
priority and give the display back with a NULL address. We do the same:
- 8888, stride 512, in P11 (4 MB; P9 belongs to the states). P11 is overwritten by the ME eDRAM
  save during every suspend, so the menu gives the display back before a save/load and captures
  and redraws afterwards.
- The frozen game's picture (`sceDisplayGetFrameBuf`, any of 5650/5551/4444/8888, read through
  the uncached alias) is converted once into a dimmed background; the UI is drawn over a copy.
- Unconfirmed: that the display scans out from P11 (0x0BC00000). Fallback: a 565 buffer in VRAM
  with a backup of what it covers (MacroFire's approach).

**Opening it: the NOTE (♪) button, tapped.** It is kernel-only, so the game never sees it, and
a long hold keeps the system's mute (PSPFatSave uses the same, `NOTE_TAP_US` 400 ms). Game buttons
in a combo would reach the game before we can freeze it. HOME comes with M3 (system HOME popup off
via `sceImposeSetHomePopup(0)`, our own Quit Game).

**Input while open:** `sceCtrlSetButtonIntercept`/masks hide the buttons from the game's user
buffers and latch while kernel reads still see them; on close wait until every button is up, then
unmask. Items: Save to RAM, Load from RAM, Resume (Phase 2 adds slots and the card). Redraw only on
change, pace with `sceDisplayWaitVblankStart`, `scePowerTick` to keep the backlight on.

**Engine:** the menu freezes the game itself, so the engine gets "caller holds the freeze"
entry points (no second freeze, no thaw). After a load, execution comes back inside the save-time
menu with `PW_ENGINE_LOADED`: the menu closes (its P11 buffers are gone) and thaws.

## Phase 2 — E3: states on the Memory Stick, loads after a reboot

**The open question first (test T1).** Nobody has reversed the Memory Stick FAT driver. After a
rollback the restored kernel's view of the card predates everything written since the save
(the state file itself, game saves, our log). Indirect evidence says a suspend/resume makes the
stack revalidate the card like a re-insertion (Inferno re-opens the ISO after a resume when a read
fails; Adrenaline's "reinsert MS" is just the MS filesystem's suspend/resume events; our capture
is taken after the driver's suspend handlers). T1 settles it: after a load, still frozen and
before any write, check that a probe file created after the save is visible.

**Scheme (risk-ranked: Memory Stick FAT corruption first):**
1. Our files never allocate clusters after the kernel capture: a slot file is created at its full
   size before the save's suspend (so the captured FAT view already has it) and then only
   overwritten in place; header + CRC written last (an interrupted write leaves an invalid slot,
   never a broken FAT).
2. Before every capture: close our files, `sceIoSync("ms0:")`.
3. A load reads the whole file into P9 while the game still runs, checks CRC + fingerprint, and
   only then freezes; the restored world never reads the card for the state.
4. After the rollback, game frozen: freshness probe. Pass: normal. Fail: no Memory Stick write
   until the next reboot (logs stay in RAM), the user is told to quit without saving.
5. Refuse when ARK's `mscache` is on (its block cache survives a resume).

**Cross-boot.** The whole kernel comes back, so plugin-set differences come back with it; what
must match is outside the image: our P9 block at the same address (0x8A000000), P2 size (no
highmem), P8 usage, Inferno's cache not taking P9, the same ISO path and file, the same unit and
firmware, and our build. Fingerprint checked before any freeze: model / Tachyon / Baryon / fuse ID,
devkit, ARK version + SE config (clocks, mscache, infernocache, highmem), module list hash, P2/P5/
P8/P9 info, build + layout version, apitype, init file, ISO path/size/mtime, disc ID. The load side
must use the save-time addresses of `g_restore_pending` and `pw_handoff_ctx` (stored in the
header), not this boot's (PSPFatSave: "the plugin loads at a different kernel address each boot").
Fields describing the current hardware (IPL seeds, live switch state, real resume info) are taken
in the load boot, never from the file.

**I/O.** 1 MB `sceIoWrite`/`sceIoRead` directly from/to P9, 64 KB-aligned offsets and lengths
(~14 MB/s on the card; unaligned 2.5-4.3 MB/s). Never DMA from/to eDRAM. No free-space devctl (it
scans the whole FAT, seconds): detect short writes instead.

**Tests:** T0 card image + PC disk check; T1 freshness after a same-session load from RAM with a
card write in between; T2 same-session load from the card; T3 quit to the XMB and back, load;
T4 other writes in between (game saves, USB) + an in-game save after the load, disk check and
hashes; T5 fingerprint refusals (changed plugin, renamed ISO, infernocache/highmem, other build);
T6 full power-off, load days later; T7 (E4) 100 cycles with a disk check every 10.
