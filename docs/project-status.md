# Project status

Last updated: 2026-10-20. Update this file whenever a milestone, a blocker or the next action
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
| Battery (BAT), Play Time (PLT) | **Implemented on branch `feature/psp-xmb-overhaul`** (PR #4, not merged): XMB → Settings → Battery, XMB → Game → Play Time (ADR-024); play time counted in the game (first file written on the device); device tests B1, P1 pending | [overhaul](psp-xmb-overhaul/README.md), [battery](subsystems/battery.md), [playtime](subsystems/playtime.md) |
| Pocketwatch Home (HOM) | **On branch `feature/psp-xmb-overhaul`** (ADR-025, ADR-026): our own home screen in the Modern style - ISO/CSO, homebrew and PS1 games with their art, play time, states, battery, card. **The PSP starts in it instead of the XMB** (XMB → Settings → Pocketwatch → Start screen; VERIFIED on the device 2026-10-18, run 2). **Drawn by the GE since ADR-028** (the CPU version was "very laggy"); HOME opens Home's own panel; USB Connection inside Home (2026-10-19). First device numbers of the GE version: 45-49 fps, GE 11-13 ms (log 2026-10-19) - backgrounds moved to VRAM, stamps 8-bit, the free-space count off the loop (ADR-029/030), checked in PPSSPP. Settings, System Information, a game's states, Saved Data and Information, Music, Photos (slideshow, wallpaper), sorting, PSN EBOOTs, key sounds and the Internet Browser in Home (2026-10-19, [m7-xmb-inventory](research/m7-xmb-inventory.md)). Home's heap had left only 512 KB to the firmware (not 4 MB, HOM-NFR-002, fixed 2026-10-19 - the keyboard, Saved Data, music and the network may have failed on the PSP for it). Device round 2026-10-19 (`78e3690`, [log](research/logs/2026-10-19-home-hm10-run2.log)): the GE now 0.4-8.5 ms a frame, but a ~300 ms stall about every second (30-36 fps, 5-7 fps while changing settings) - the slow reads moved off the frame loop (ADR-031). Round 3 (`c3f3699`, [log](research/logs/2026-10-19-home-hm10-run3.log)): **59-60 fps, CPU about 1 ms a frame** (ADR-031 VERIFIED; left: one 209 ms first build of the Settings rows); USB in Home did not connect (fixed: activated once as ARK-4's menu, Connect Again by hand); the style setting did not change Home and Music, Photos, Play Time and Settings looked bad (owner) - answered: the three styles in Home, Classic as the XMB with the system's pictures (ADR-033), HM20-HM21. Round 4 (`b5cab03`, [log](research/logs/2026-10-19-home-round4.log)): USB in Home failed beside RemoteJoyLite (`80243001` / `80243006`; fixed, ADR-034); 5 of 11 system pictures and no sky in Home (the plugin reads them: now each read is logged); asked for: categories and pages that slide, themes, colours, the battery's percent, backgrounds, Home's own Saved Data, music covers and seeking, photo albums by game and an editor - done 2026-10-20 (ADR-035, ADR-036; PPSSPP 57-60 fps), HM22-HM25 | [home](subsystems/home.md), [m7-full-ui](research/m7-full-ui.md), [m7-boot-shell](research/m7-boot-shell.md) |
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

## Device results 2026-10-19, round 3 (owner, build `c3f3699`)

- Home: 59-60 fps with 0-5 late frames in 30 s ([log](research/logs/2026-10-19-home-hm10-run3.log)); the
  battery's cycle count costs 35 ms (on its thread), the first Settings rows 209 ms (once).
- HOME in Need for Speed: Most Wanted froze the game for good (the next session opened the hub):
  a frozen thread inside a card read held the FAT driver (ADR-032, SHL-REQ-013, test H17).
- USB Connection in Home did nothing on the PC; the XMB's worked (HOM-REQ-011 changed: no
  automatic re-activation; `home usb:` lines in `home.log`).
- Changing the style changes nothing in Home (HOM-REQ-006 was still planned); Music, Photos, Play
  Time and Settings "look bad, not like they should".

## Device results 2026-10-20, round 5 (owner, build `cee6786`, spoken; no log sent)

- HOME → Screenshot works and the picture is in All Photos; its name (`ULUS10277_<date>_<time>`)
  did not fit and did not say which game it came from. Answered (ADR-039): a game's pictures are
  named by date and time under the game's title, and the game's page lists its Photos.
- Play Time: a second of Castlevania showed nothing. Cause (code): Home listed only games with
  a minute or more, and a session under a second is never written. Answered: seconds are shown.
- Music: no way found to seek, no hint of the keys; covers slow, track changes lag. Answered:
  left / right seek, up / down the track, both in the hints; the playing track tagged first;
  `home.log` now times a cover and a track's start (RESEARCH REQUIRED: the numbers, HM24).
- Photos: right "after the third" went "somewhere". Cause (code): Modern's grids scrolled by the
  selection divided by the columns (a quarter row per step). Fixed. If the owner was in Classic,
  right at the top of Photos is the next section (as in the XMB) - OPEN QUESTION, HM25.
- The nickname's firmware keyboard and its question should be Pocketwatch's, "in games and in
  the menu". Home: its own keyboard (HOM-REQ-030). Games: OPEN QUESTION (ADR-039: a separate
  PRX, the kernel plugin has 2 KB left).
- Not built for the PSP in this change: the WSL toolchain (`~/pspdev`) was missing on the
  machine; host tests, previews and the format check pass.

## Device results 2026-10-20, round 6 (owner, build `ead1320`, Modern, [log](research/logs/2026-10-20-home-round6.log))

- Photos still "jerky, jumps, I cannot look through a folder", and "micro freezes". Causes
  (code, reproduced in PPSSPP with the owner's 15 photos in 7 folders): the albums' grid had
  one row on screen while its scroll assumed two (the choice went off screen after the third
  album), and the grids' scroll waited for the selection's motion, then jumped. Fixed (ADR-040).
- The log: 500-1035 ms frames (about one in 30 s) and 10-30 late frames in 30 s, no part named;
  RESEARCH REQUIRED: the next `home.log`'s `slow:` entries (ADR-040). No `home music:` line:
  music was not tested in this round.
- The card's clock: the PSP reads 2011-01-16/17 (`xmb.log`, the files' dates) - lost and passed
  through the firmware's date screen (see below, ADR-041).

## 2026-10-20: the clock, the battery, the performance overlay (owner's requests)

- **Clock** (ADR-041, HOM-REQ-031/032, [m7-clock](research/m7-clock.md)): implemented in Home -
  a lost clock (kernel error, or before 2016) set at the start to the newest file time; then SNTP
  over Wi-Fi, by itself after a restore and by hand in Settings → Date and Time. Host tests;
  PPSSPP: the "Wi-Fi switch is off" answer. Not run on the PSP; the network in Home is RESEARCH
  REQUIRED (the browser's `sceNetInit` failed there with 0x8002013A; the clock's path is NTPSP's).
- **Battery** (ADR-042, BAT-REQ-008..010, [m7-battery-calibration](research/m7-battery-calibration.md)):
  Home's Battery page shows the current and our estimate; a calibration run measures the
  capacity the battery delivers. Host tests; PPSSPP: the page and the run's first steps.
- **Performance overlay** (ADR-043, PRF-REQ-001..005, [perf-overlay](subsystems/perf-overlay.md)):
  over Home (PPSSPP checked: 59 fps, the frame's CPU and GE time, clocks, memory); in games
  `pocketwatch_perf.prx` (perf/, 9.7 KB loaded, `make check` passed), turned on in Home's
  Settings through PLUGINS.TXT (PPSSPP: the line written and turned off again). Not run on the PSP.
- Budgets unchanged: game plugin 194 552 / 196 608 (debug), XMB plugin 65 261 / 65 536; Home
  1 621 916 / 2 097 152.

## Device results 2026-10-20, round 7 (owner, build `615e6d5`, [log](research/logs/2026-10-20-round7-home.log))

- **The clock was put back** at the first start (VERIFIED: `home clock: lost (0x00000000,
  1295231352 s); the card's newest time 1792512064 s of 13 files`, set and read back; the files
  since are dated 2026-10-20). HM29's internet part: no saved connection (`profile=0`).
- **"The overlay does not work in the game"**: `pocketwatch_perf.prx` was not on the card - Home
  put `seplugins/pocketwatch/` back from its cache after USB (VERIFIED by chkdsk's lost chain;
  ADR-046, fixed: the cache dropped).
- **"Wi-Fi does not work"**: no connection saved, and Home's network was broken by its own build
  (VERIFIED: the `sceNetInet` import table over `sceNet`'s; ADR-044, fixed, `make check` guards
  it). Home now searches networks and makes connections (HOM-REQ-033, [m7-wifi](research/m7-wifi.md)).
- Asked for: the calibration as a whole screen (BAT-REQ-011), more brightness and a volume and
  brightness indicator in the menu and games (ADR-045, HOM-REQ-034, PRF-REQ-006..008) - done the
  same day; previews `home-calibration.png`, `home-osd.png`; PPSSPP: the Wi-Fi search, the
  settings' files.

## Device results 2026-10-21, round 9 (owner, build `b91f3e3`, `home.log`, chkdsk)

- "Pocketwatch does not start with the console; in games neither the hub nor the overlay": the
  card's `pocketwatch.prx` and `pocketwatch_xmb.prx` were broken - Home's write after USB came
  from the stale FAT tables again, because ADR-046's cache drop is refused to a user program
  (`cache dropped 800200D1`). VERIFIED by chkdsk ("first allocation unit is not valid", 5 lost
  chains). Fixed: after USB Home starts again before any write (ADR-048). The card: `chkdsk /F`,
  then a deploy.

## Device results 2026-10-20, round 8 (owner, build `f36f0f5`, spoken and `home.log`)

- "The overlay and the indicator are still not seen in games": `pocketwatch_perf.prx` was lost
  once more - the old Home (615e6d5) ended the deploy's USB (VERIFIED: its USB line without "cache
  dropped", chkdsk a second lost chain of 32 KB); the new Home's USB line carries it. Redeployed.
- VERIFIED on the device: Home's network comes up now - Search for Networks found 2
  (`home net: search 0 ... seen=2`); no connection made yet. New: the browser's
  `sceUtilityHtmlViewerInitStart` 0x800200D9 (OPEN QUESTION, not asked for now).
- Asked for: no brightness slider in the XMB; remove bright3 and do its job in Pocketwatch; the
  numbers overlay "terrible, not understandable" - done (ADR-047): Own Steps, the module in the
  XMB, the overlay redrawn; bright3's lines off, its levels moved to `perf.txt`.

## Next actions (in order)

0. **Owner (round 10, after `chkdsk /F` and a deploy):** HM32 first (USB: Home starts again
   after it), then the round 9 list. **Round 9 (still):** PF4 (Own Steps in Home, the XMB, a game), PF1 and PF3 (the overlay and the
   indicator in a game), HM33 (Wi-Fi: make the connection), HM32 (USB, now with the new Home);
   send `home.log`. **Before (round 8):** deploy, then **turn the PSP off from Home's USB screen** (the old Home's
   exit from USB would undo the deploy once more); HM32 (USB), HM33 (Wi-Fi: make the home network's
   connection), HM29 again (the internet's time), HM34 and PF3 (the indicator, the boost), PF1
   (the numbers in games), HM30 (the calibration's screen), PF2; send `home.log`,
   `pocketwatch.log`. **Before (round 7):** deploy (`scripts\deploy.cmd`), then HM29 (the clock: the first start
   should fix the 2011 date), HM31 and PF1 (the overlays; overlay off again before state tests),
   HM30 (a calibration run, hours), PF2; and HM25 again (ADR-040) with `home.log`; HM24, HM27, HM28.
   **Before that (round 6):**
   **owner:** HM24, HM25, HM27, HM28 (ADR-039) with `home.log`; name the games that open the
   PSP's keyboard (for G1 of [m7-game-keyboard](research/m7-game-keyboard.md), and choose its
   option A / B / C). Then:
   **Owner (2026-10-20, build `cee6786` on the card, ADR-034..038):** HM12 (USB from Home with
   RemoteJoyLite on), HM22 (the settings' categories and pages, the themes, accents and
   backgrounds), HM23 (a game's Saved Data and Save States - delete only a copy), HM24 (music:
   tags, covers, seeking, repeat and shuffle), HM25 (albums, the editor), HM26 (HOLD, the sleep
   timer, the clock, favourites, the Aurora), H18 (HOME → Screenshot), then H17, HM20-HM21 and
   HM11, HM13-HM19; send `home.log` (its `home art:` line names every flash0 read) and
   `pocketwatch.log`. Then the list below.
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
