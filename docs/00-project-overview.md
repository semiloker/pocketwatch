# Pocketwatch — project overview

> **P**ause. **S**ave. **P**lay.

## Vision

Pocketwatch is a kernel-mode plugin for PSP custom firmware that adds features the PSP never had,
built so that they look and behave as if Sony had shipped them in the firmware. In watchmaking
every feature beyond telling the time is a *complication*; each Pocketwatch feature is one. The
first complication is **States**: freeze any retail game at any moment, keep that moment, and
return to it later, from an in-game menu that looks like the XMB and with states stored as native
saved data.

## Owner's priorities (in order)

| # | Priority | What it means in practice |
|---|---|---|
| 1 | **Native 1:1** | The in-game menu opens on HOME (PS) like the system's own, uses the system's fonts, icons, sounds and button conventions read from the user's PSP, and states show up in the XMB's Saved Data Utility. A user should not be able to tell it is a plugin. |
| 2 | **Optimisation, low level** | Save and load in seconds, no measurable cost while the game runs, a tiny kernel-RAM footprint (P1 has ~300 KB free in games, **VERIFIED** [M1](research/m1-hardware.md)). |
| 3 | **Easy extension** | New complications plug into the menu and the core services without touching the engine ([complication framework](subsystems/complications.md)). |
| 4 | **Beauty and reliability** | Never corrupt the Memory Stick, the flash or the user's saves; refuse with a native message rather than risk it. |

## Target platform

| | |
|---|---|
| Device | **PSP-3000** (the only tested model, **VERIFIED** `sceKernelGetModel() = 3`). PSP-2000, PSP Go and PSP Street have 64 MB as well and may work later (**INFERRED**, untested). The PSP-1000 (32 MB) is out of scope for States. |
| Firmware | ARK-4 on OFW 6.61 ([ADR-002](architecture/decisions.md#adr-002--target-ark-4-on-661-psp-3000)). Other CFWs are untested. |
| Runlevel | `UMD` (retail games from UMD or ISO); homebrew is excluded. |
| Toolchain | PSPDEV (psp-gcc), kernel libraries only, no newlib. |

## Scope

In scope (see the [roadmap](roadmap.md) for order and status):

- **States engine** — save/load the complete game state (game RAM, eDRAM, scratchpad, the 4 MB
  kernel image, P8, the ME eDRAM save) within a boot (done) and across reboots (E3).
- **In-game shell** — HOME opens Pocketwatch's hub over the frozen, dimmed game and replaces the
  system HOME screen, including Quit Game; guards refuse unsafe moments.
- **XMB-style UI** — hub, state list in the style of the Saved Data Utility, dialogs, with the
  system's own PGF fonts and RCO icons read at runtime.
- **Native storage** — each state is a hashed SAVEDATA folder `PW<GAMEID><NN>` with icon, picture
  and detail text, browsable and deletable from the XMB.
- **XMB integration** (later) — a VSH-side entry to continue a game from a state.
- **Feasibility research** — whether an independent firmware/software stack for PSP hardware is
  realistic ([firmware study](firmware/independent-firmware-feasibility.md)); research only.

## Non-goals

- Supporting the PSP-1000, homebrew apps, PS1 (POPS) games or other CFWs in v0.1.
- Running together with another savestate engine (PSPStates, Custom Home states).
- Redistributing any Sony file (fonts, icons, sounds, firmware modules): they are read from the
  user's own PSP at runtime.
- Network features, cheats, overclocking UIs (possible future complications, not planned).
- A firmware rewrite as a goal in itself: the long-term property sought is control and
  extensibility, reached by the cheapest level that delivers it.

## Success criteria for v0.1.0 (from the release gate, [roadmap M6](roadmap.md#m6--hardening-v010))

- 100 save/load cycles on 5 games without a failure, the Memory Stick FAT clean afterwards
  (PC check) and the system settings intact.
- Every refusal (corrupted file, full card, low battery, sleep with the menu open, HOME on a
  loading screen, game saving) shows a native message and leaves the system unharmed.
- The menu is indistinguishable in style from the system UI by the owner's judgement against the
  reference set ([UI spec](ui/README.md)).

## Where to go next

- [Documentation index](README.md) — every subsystem and its status.
- [System architecture](01-system-architecture.md) and [system map](02-system-map.md).
- [Project status](project-status.md) — what is happening now.
