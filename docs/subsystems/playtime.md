# Play Time — specification

| | |
|---|---|
| Area code | `PLT` |
| Status | Counting in the game and the XMB page (Game → Play Time, ADR-024) implemented 2026-10-17 (branch `feature/psp-xmb-overhaul`); the first file was written on the device 2026-10-17 (1 s of play in a 20-minute session spent in the hub: correct); device test P1 pending |
| Code | [`core/playtime.c`](../../plugin/src/core/playtime.c) (portable table and file format), [`complications/playtime/playtime.c`](../../plugin/src/complications/playtime/playtime.c) (counting, writing), [`vsh/src/xsettings.c`](../../vsh/src/xsettings.c) (the XMB page) |

## A. Overview

- **Purpose:** how long each game has been played, honestly: only what Pocketwatch saw.
- **Current state:** XMB → Game → Play Time: the total of all games, then each game (most
  played first, up to 16), and a note that only time with Pocketwatch running is counted.
- **Target state:** per-game details (sessions, average, longest, recorded since; the file has
  them); weekly / monthly summaries (PROPOSED: needs a session log, not only totals).

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `PLT-REQ-001` | Play time counts only while the game runs with the hub closed: the hub, saves and loads, and sleeps are not play. | P1 | Implemented: the shell's once-a-second tick, each step at most 2.5 s; no tick while the hub is open or the engine busy | code review, device test P1 |
| `PLT-REQ-002` | A state load does not lose or double the time played since the save. | P1 | Implemented: the session's counters are in a P9 block the rollback does not touch (its address is in the state fingerprint, extra[3]) | device test P1 |
| `PLT-REQ-003` | Totals survive reboots and a write cut short: two files written in turn, each with a sequence number and a CRC; the newer complete one is read. | P1 | Implemented | `playtime_round_trip_and_crc` |
| `PLT-REQ-004` | A session adds to the game's total once, however often it is written (every 5 minutes of play, after each hub visit, at Quit Game). | P1 | Implemented: total = the record at the boot's start + this session | `playtime_sessions_add_to_the_boot_s_base_once` |
| `PLT-REQ-005` | Nothing before the first recorded session is claimed: "Recorded since" is the first session Pocketwatch saw; the page says only Pocketwatch's time is counted. | P1 | Implemented | `playtime_sessions_add_to_the_boot_s_base_once`, review |
| `PLT-REQ-006` | Games are identified by their disc ID (titles can repeat across regions); the title is the one of the last session. A damaged line costs that game, not the file; 100 games at most, the least recently played making room. | P2 | Implemented | `playtime_bad_lines_cost_only_themselves_and_a_full_table_drops_the_oldest` |
| `PLT-REQ-007` | Time after the last write is lost when the game ends without Quit Game (its own menu, a crash, power loss): at most 5 minutes. | P2 | Accepted limitation (module_stop does no I/O) | - |

## C. Architecture and integration

A complication: `init` reads the table once (P11 scratch, freed at once), keeps this game's
record at the boot's start in the P9 live block; `tick` adds play and writes (every 5 minutes of
play, and once after each hub visit: `on_open` only asks for it, so the hub does not open later);
`on_quit` (Quit Game) writes. The XMB plugin reads the newer complete file when Game →
Play Time opens and lists the games by total. Files: `seplugins/pocketwatch/playtime-a.txt` / `playtime-b.txt`, tab-separated
`id total_s sessions longest_s first last title`, header `# Pocketwatch play time 1 seq N`,
trailer `# crc XXXXXXXX`. Dates are the PSP's own clock (as set by the owner), shown in the
system's date format; durations never come from the clock.

## D. User experience

Durations as "12 h 30 min" / "25 min" in the user's language; the list of games by total time.

## E. Reference material

None external; the design follows the project's write rules (CLAUDE.md: no card writes during a
save's freeze-resume window, none in module_stop).

## F. Implementation plan

1. ~~Counting, files, hub row, page~~ (2026-10-17).
2. P1 on the device.
3. A session log (date, game, duration) for weekly / monthly summaries; export.
4. The XMB statistics page.

## G. Testing and acceptance

Host: `make -C tests run T=playtime`. Device: testing guide P1. Accepted when the totals after a
session of known length (with a save, a load, a sleep and hub visits) are within a minute of the
time actually played.

## H. Risks and decisions

[ADR-023](../architecture/decisions.md#adr-023--play-time-from-the-shells-tick-kept-in-p9-written-to-two-alternating-files);
[RISK-005](../risk-register.md#risk-005--kernel-ram-and-prx-size-growth).
