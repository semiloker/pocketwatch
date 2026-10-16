# Project status

Last updated: 2026-10-16. Update this file whenever a milestone, a blocker or the next action
changes ([workflow](AI_WORKFLOW.md#4-keeping-documentation-true)).

## Where we are

| Area | State | Evidence |
|---|---|---|
| States engine (E1, E2, fast mode) | **Done** — save → play → load in the same boot on the PSP-3000; first save 7.8 s, later saves 4.2 s, loads 3.9 s (Castlevania) | [m2-speed](research/m2-speed.md), speed run 7 |
| Minimal menu (M3 phase 1) | **Done** — NOTE tap, Save/Load/Resume over the dimmed game, device-verified | [log 2026-10-16](research/logs/2026-10-16-menu1-castlevania.log) |
| Documentation system (DOC-1) | **In progress** — workflow, index, architecture, specs, roadmap, risks, UI spec + concepts, firmware study written; awaiting the owner's review | this change |
| HOME takeover + Quit Game (M3a) | **Implemented 2026-10-16, device test pending** | [m3-home-takeover](research/m3-home-takeover.md#7-implementation-and-device-test) |
| Native look (M4) | **In progress**: hub concept A with the system font (ltn0), fades, dialogs implemented; RCO icons, i18n, state list next | [ui](ui/README.md) |
| E3 states on the card | **In progress**: T1 experiment implemented (debug hub item), device run pending | [m3-menu-e3](research/m3-menu-e3.md#t1-device-protocol) |
| Firmware feasibility | **Research** — FW-G1 partial; the research pass of 2026-10-16 stopped early | [feasibility study](firmware/independent-firmware-feasibility.md) |

The documentation system was committed on the owner's request (`0aaa205`).

## Next actions (in order)

1. **Owner:** device test of the HOME hub, Quit Game and T1 ([protocol](research/m3-home-takeover.md#7-implementation-and-device-test)); say whether concept A (now built) is right or B is wanted, and whether "Pocketwatch" should appear in the hub.
2. **Dev, after the test:** fix what it shows; then E3 storage on the T1 answer; RCO icons
   (RLZ/RCO/GIM decoders); state list (M5); complication registry; i18n.

## Blockers and open questions

| Item | Type | Owner |
|---|---|---|
| Hub layout A vs B (A built by default, [ADR-016](architecture/decisions.md#adr-016--hub-concept-a-by-default)); branding in the hub | OPEN QUESTION | Owner |
| Does the Memory Stick FAT driver revalidate the card on resume? (T1) | RESEARCH REQUIRED | Dev + Owner (card backup, PC check) |
| How to keep ARK-4's exit combos from rebooting while a state is held (`SHL-REQ-012`) | RESEARCH REQUIRED | Dev |
| Real in-game HOME screen look on 6.61 (capture) | RESEARCH REQUIRED | Dev (capture tool) + Owner |
| Which `topmenu_icon.rco` labels are which icons | RESEARCH REQUIRED | Dev (dump tool) |
| Firmware study: re-verify REPORTED boot/recovery claims with primary sources | RESEARCH REQUIRED | Dev |

## Known documentation/code mismatches

| Mismatch | Resolution |
|---|---|
| `CHANGELOG.md` `[Unreleased]` still describes FastLZ and lacks LZ4, fast mode, the menu | rewrite at the next release cut (M6) — system map finding A7 |
| `research/m1-hardware.md` / `m2-engine.md` name removed spike sources | historical; left as is (finding A8) |
| `sfo.c`, `savedata_hash.c`, `png.c` are linked but unused until M5 | accepted; counted in the size budget (finding A4) |

## How to resume in a new session

Read `CLAUDE.md` (it imports [AI_WORKFLOW.md](AI_WORKFLOW.md)), then this file, then the spec of
the subsystem in "Next actions". Build and test commands are in `CLAUDE.md`; device protocols are
in the research notes.
