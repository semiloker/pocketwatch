# Project status

Last updated: 2026-10-16. Update this file whenever a milestone, a blocker or the next action
changes ([workflow](AI_WORKFLOW.md#4-keeping-documentation-true)).

## Where we are

| Area | State | Evidence |
|---|---|---|
| States engine (E1, E2, fast mode) | **Done** — save → play → load in the same boot on the PSP-3000; first save 7.8 s, later saves 4.2 s, loads 3.9 s (Castlevania) | [m2-speed](research/m2-speed.md), speed run 7 |
| HOME takeover + Quit Game (M3a) | **Done on the device** (2026-10-16): HOME opens the hub, Quit Game returns to the XMB | owner report, log 2026-10-16 |
| T1 (FAT freshness after a rollback) | **Passed** for a state saved through a real sleep; T1b (touch-and-go save) pending | [m3-menu-e3](research/m3-menu-e3.md#t1-device-protocol), [ADR-017](architecture/decisions.md#adr-017--state-files-are-written-after-the-capture) |
| Native look (M4) | **Implemented, owner review pending**: XMB background and wave, ltn0, RCO icons and button pictures, clock/battery, the system language (7 translations + Ukrainian, ADR-018) | [ui](ui/README.md), [m4-native-icons](research/m4-native-icons.md) |
| States on the card (E3) + state list (M5) | **Implemented and host-tested, device tests pending** | [states storage](subsystems/states-storage.md), [testing guide](testing.md#4-states-on-the-memory-stick) |
| Complication framework (CPL) | **Implemented** 2026-10-17: the hub builds its rows from `complications/table.c` (System, States, Settings, debug card test); host-tested, device run of the refactored hub pending | [complications](subsystems/complications.md) |
| UI styles (ADR-019) | **Implemented, device review pending**: Classic (XMB, default), Modern, Minimal; XMB → Settings → Pocketwatch → Style | [styles](ui/styles.md), [testing guide](testing.md#settings-in-the-xmb-since-adr-024) ST5 |
| Settings (CPL-REQ-006) | **Implemented, device tests pending**: Fast Save and Load, Clock and Battery, Style, Language; changed in the XMB (ADR-024), `config.txt` | [complications](subsystems/complications.md), [testing guide](testing.md#settings-in-the-xmb-since-adr-024) |
| Guards (M3b) | **Implemented, device tests pending**: battery, USB, WLAN, highmem, Custom Home / PSPStates; redraw after a sleep | [game shell](subsystems/game-shell.md), [testing guide](testing.md#7-guards) |
| Release preparation (M6) | **In progress**: release build switch, changelog rewritten, testing guide, [compatibility matrix](compat.md) prepared for the owner's 22 games; filling it and the 100-cycle gate need the device | [roadmap](roadmap.md#m6--hardening-v010) |
| XMB integration (M7) | **In progress**: variant B chosen (ADR-020); the GAME side (load a slot named in `continue.txt` once the game runs) implemented, device test V1 pending; the XMB plugin (`vsh/`, ADR-021) adds Settings → Pocketwatch with the XMB's own option list - builds and passes `make check`, device test X1 pending; the game's △ menu entry next | [vsh-integration](subsystems/vsh-integration.md) |
| Battery (BAT), Play Time (PLT) | **Implemented on branch `feature/psp-xmb-overhaul`** (not merged, not committed): XMB → Settings → Battery, XMB → Game → Play Time (ADR-024); play time counted in the game (first file written on the device); device tests B1, P1 pending | [overhaul](psp-xmb-overhaul/README.md), [battery](subsystems/battery.md), [playtime](subsystems/playtime.md) |
| Pocketwatch Home (HOM) | **On branch `feature/psp-xmb-overhaul`** (ADR-025, ADR-026): our own home screen in the Modern style - ISO/CSO, homebrew and PS1 games with their art, play time, states, battery, card. **The PSP can start in it instead of the XMB** (XMB → Settings → Pocketwatch → Start screen; ARK-4's launcher field set by the XMB plugin, with a start guard). Builds, host-tested; device test HM1-HM9 pending | [home](subsystems/home.md), [m7-full-ui](research/m7-full-ui.md), [m7-boot-shell](research/m7-boot-shell.md) |
| Documentation system (DOC-1) | **Done** (kept current with every change) | this file |
| Firmware feasibility | **Research** — FW-G1 partial | [feasibility study](firmware/independent-firmware-feasibility.md) |

## Device results 2026-10-17 (owner)

- Castlevania: save to slot 1 worked (13.5 MB, 7.5 s), Settings and styles switch, Quit Game ok.
- The 3rd Birthday: the Memory Stick Test's save rebooted the PSP into the XMB during its sleep
  (RISK-016, test M13).
- The menu: very low frame rate in Classic and Modern, no animation in the state lists, the
  battery icon and some screens not like the XMB, the hub not following the XMB's theme colour.
  Answered in this change (frame cost, time-based motion, the XMB's own battery picture, option
  lists like the XMB's); the theme colour already selects the sky - the log now says which one
  (test H12).

Second run (2026-10-17, [log](research/logs/2026-10-17-hub-fps-castlevania-persona.log)): the
hub's first fps lines (Modern 19-30 fps, Classic 11-16 fps, 17-21 ms of drawing), a second
Castlevania slot save (ok), Persona hub and Quit (ok). The sky follows the theme colour (colour 1
-> sky 0); the PSP's own clock says January (2011), which is also the month the default uses.
Answered: half the wave per frame, rasterised icons, drawing only on change, a frame split and a
per-thread CPU line in the log.

Third run (2026-10-17, build be14f66, [log](research/logs/2026-10-17-home-dead-latch.log)): HOME
showed nothing at all (the system HOME screen is off by design and the hub never opened). Cause:
the SHL-REQ-012 change read the buttons from the controller's latch, whose press bits stay set
until someone reads the latch, so the release of HOME was never seen. Fixed: the buttons come
from `sceCtrlPeekBufferPositive2` (same buffer, not hooked by ARK-4); device check pending.

Fourth run (2026-10-17, build a03f2b4): HOME opens the hub again (log `ctrl: buttons through
sceCtrlPeekBufferPositive2`, hub 8.7 s at 143 frames). Owner: L+R+START+DOWN in the game turned
the PSP off instead of starting ARK's launcher (`ARK_01234/VBOOT.PBP` exists, so ARK starts it);
nothing of ours runs on that path (no log line after the hub) - RESEARCH REQUIRED: the same combo
with Pocketwatch off in PLUGINS.TXT (testing guide SH2). Also: the hub opened late (answered: it
opens on the press, the slot count is kept, each open is timed, H16) and a volume bar was asked
for (UI-REQ-016, H15). Next asked: a Pocketwatch entry in the XMB's settings (VSH-REQ-006).

Fifth run (2026-10-17, build d2579ea, [log](research/logs/2026-10-17-t3-refused-castlevania.log)) -
the first T3: Castlevania saved to slot 1 (real sleep, 8.0 s, 18.5 MB on the card), Quit Game,
started again, Load State → slot 1: refused, "saved with another setup". Cause (VERIFIED by
reading the code, regression test `store_loads_a_state_from_an_earlier_boot`): the store compared
the saved environment with `pw_engine_env_now` before the engine had staged, so the P9 block
address and the P8/ME partition fields were 0 in a fresh boot - every state from an earlier boot
was refused; same-boot loads passed because the save had staged. Fixed: `pw_engine_env_for_load`
measures and stages first; a refusal now logs each differing field. T3 must be run again. Also:
the hub opened 291-308 ms after the press (UI-NFR-001 target 300 ms; 262-274 ms of it is
`screen`: the frame capture and the backdrop). No `xmb.log` appeared (see X1).

## Next actions (in order)

1. **Owner:** run the [testing guide](testing.md) sections 1-8 with a card backup and send the
   log and the results (section 9).
2. **Dev, meanwhile:** M7 XMB side: research the 6.61 XMB's △ menu (call sites, how xmbctrl
   chains, the selected item's path), then the VSH PRX ([vsh-integration](subsystems/vsh-integration.md#f-implementation-plan)).
3. **Dev, after the results:** fix what they show; then, with the [compatibility
   matrix](compat.md) filled and the 100-cycle gate passed, the v0.1.0 tag. Release build: 94 % of its PRX
   budget (RISK-005).

## Blockers and open questions

| Item | Type | Owner |
|---|---|---|
| T1b: is the card view fresh after rolling back a touch-and-go save? (the marker check guards it meanwhile) | RESEARCH REQUIRED | Owner (device run) |
| What else differs between boots in the kernel image (cross-boot loads, T3/T6) | RESEARCH REQUIRED | Owner (device run) + Dev |
| Hub layout A vs B (A built by default, [ADR-016](architecture/decisions.md#adr-016--hub-concept-a-by-default)) | OPEN QUESTION | Owner |
| TV-out with states (not guarded) | OPEN QUESTION | Dev |
| Firmware study: re-verify REPORTED boot/recovery claims with primary sources | RESEARCH REQUIRED | Dev |

## Known documentation/code mismatches

| Mismatch | Resolution |
|---|---|
| `research/m1-hardware.md` / `m2-engine.md` name removed spike sources | historical; left as is (finding A8) |
| `research/m3-menu-e3.md` describes the preallocation scheme of the E3 design | superseded by ADR-017; the note stays as the design history |

## How to resume in a new session

Read `CLAUDE.md` (it imports [AI_WORKFLOW.md](AI_WORKFLOW.md)), then this file, then the spec of
the subsystem in "Next actions". Build and test commands are in `CLAUDE.md`; device protocols are
in the [testing guide](testing.md).
