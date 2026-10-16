# Project status

Last updated: 2026-10-16. Update this file whenever a milestone, a blocker or the next action
changes ([workflow](AI_WORKFLOW.md#4-keeping-documentation-true)).

## Where we are

| Area | State | Evidence |
|---|---|---|
| States engine (E1, E2, fast mode) | **Done** — save → play → load in the same boot on the PSP-3000; first save 7.8 s, later saves 4.2 s, loads 3.9 s (Castlevania) | [m2-speed](research/m2-speed.md), speed run 7 |
| Minimal menu (M3 phase 1) | **Done** — NOTE tap, Save/Load/Resume over the dimmed game, device-verified | [log 2026-10-16](research/logs/2026-10-16-menu1-castlevania.log) |
| Documentation system (DOC-1) | **In progress** — workflow, index, architecture, specs, roadmap, risks, UI spec + concepts, firmware study written; awaiting the owner's review | this change |
| HOME takeover + Quit Game (M3a) | **Planned, research done** | [m3-home-takeover](research/m3-home-takeover.md) |
| Native look (M4) | **Planned, research done** (PGF, RCO/RLZ/GIM, metrics) | [m4-native-look](research/m4-native-look.md), [m4-native-icons](research/m4-native-icons.md) |
| E3 states on the card | **Planned**; design written; T1 decides the write policy | [m3-menu-e3](research/m3-menu-e3.md) |
| Firmware feasibility | **Research** — FW-G1 partial; the research pass of 2026-10-16 stopped early | [feasibility study](firmware/independent-firmware-feasibility.md) |

Uncommitted in the working tree (2026-10-16): the whole documentation system (owner asked not to
commit it automatically), the phase-1 status note in `research/m3-menu-e3.md` and the archived menu
test log.

## Next actions (in order)

1. **Owner:** review the documentation and the UI concepts; choose hub concept A or B; decide
   whether "Pocketwatch" appears in the hub ([UI spec §H](ui/README.md#h-risks-and-decisions)).
2. **Dev, M3a:** HOME takeover and Quit Game per [game shell](subsystems/game-shell.md) (shell module,
   popup ownership, dialog check, quit sequence, engine owner lock `STE-REQ-011`); device protocol.
3. **Dev, M4:** PGF decoder + text, RLZ/RCO/GIM decoders with synthetic host tests; theme table;
   hub rewrite on the complication registry; debug captures of the real HOME screen/XMB for
   comparison.
4. **Dev, E3:** T1 build (FAT freshness after a rollback) → storage layer.

## Blockers and open questions

| Item | Type | Owner |
|---|---|---|
| Hub layout A vs B; branding in the hub | OPEN QUESTION | Owner |
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
