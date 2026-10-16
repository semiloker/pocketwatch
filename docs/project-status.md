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
| Guards (M3b) | **Implemented, device tests pending**: battery, USB, WLAN, highmem, Custom Home / PSPStates; redraw after a sleep | [game shell](subsystems/game-shell.md), [testing guide](testing.md#7-guards) |
| Release preparation (M6) | **In progress**: release build switch, changelog rewritten, testing guide; compatibility matrix and the 100-cycle gate need the device | [roadmap](roadmap.md#m6--hardening-v010) |
| Documentation system (DOC-1) | **Done** (kept current with every change) | this file |
| Firmware feasibility | **Research** — FW-G1 partial | [feasibility study](firmware/independent-firmware-feasibility.md) |

## Next actions (in order)

1. **Owner:** run the [testing guide](testing.md) sections 1-8 with a card backup and send the
   log and the results (section 9).
2. **Dev, after the results:** fix what they show; then a Settings item (slot count, fast mode),
   the compatibility matrix (`docs/compat.md`) from the owner's ISOs, and the v0.1.0 tag. The
   debug build is at 98 % of its size budget (RISK-005): move debug code out first.

## Blockers and open questions

| Item | Type | Owner |
|---|---|---|
| T1b: is the card view fresh after rolling back a touch-and-go save? (the marker check guards it meanwhile) | RESEARCH REQUIRED | Owner (device run) |
| What else differs between boots in the kernel image (cross-boot loads, T3/T6) | RESEARCH REQUIRED | Owner (device run) + Dev |
| Hub layout A vs B (A built by default, [ADR-016](architecture/decisions.md#adr-016--hub-concept-a-by-default)) | OPEN QUESTION | Owner |
| How to keep ARK-4's exit combos from rebooting while a state is held (`SHL-REQ-012`) | RESEARCH REQUIRED | Dev |
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
