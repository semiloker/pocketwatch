# AI workflow — how Claude Code works on Pocketwatch

This file is imported by the root [`CLAUDE.md`](../CLAUDE.md), so it loads in every Claude Code
session regardless of the account in use. It is the full, project-specific procedure; `CLAUDE.md`
keeps only the short rules. The repository, not a conversation or an account memory, is the
source of truth.

## 1. Start of every session

1. Read [`docs/README.md`](README.md) (documentation index) and
   [`docs/project-status.md`](project-status.md) (current state, next actions).
2. Before working on a subsystem, read its specification (linked from the index) and the research
   notes it links. Before changing code, read the code: documentation can lag behind.
3. Resume unfinished work from [`project-status.md`](project-status.md), the
   [roadmap](roadmap.md) and the [backlog](backlog.md), not from conversation history.
4. If the documentation and the code disagree, the code shows what *is*, the approved
   specification shows what *should be*. Record the mismatch in `project-status.md` and fix
   whichever is wrong; never silently pick one.

## 2. Evidence labels

Every claim about the system, the hardware or the firmware that is not obvious from the code uses
one of these labels (inline, e.g. **VERIFIED** (E2 run 7 log)):

| Label | Meaning |
|---|---|
| **VERIFIED** | Supported by the implementation, a device log in `docs/research/logs/`, or a cited reliable source. Say which. |
| **INFERRED** | A reasonable conclusion that has not been confirmed. Say from what. |
| **PROPOSED** | A design or recommendation that is not implemented. |
| **OPEN QUESTION** | A decision that needs information or the owner's choice. |
| **RESEARCH REQUIRED** | A feasibility question or claim that needs external evidence or a device test. |

Never present a proposal as implemented, never invent measurements, interfaces, sources or
results. A device result is VERIFIED only when the log or the owner's report says so.

## 3. Where things live

| What | Where |
|---|---|
| Index, status of every subsystem | [`docs/README.md`](README.md) |
| Vision, scope, non-goals | [`docs/00-project-overview.md`](00-project-overview.md) |
| Architecture, memory/thread model | [`docs/01-system-architecture.md`](01-system-architecture.md) |
| Subsystem inventory and interaction map | [`docs/02-system-map.md`](02-system-map.md) |
| Milestones, priorities, acceptance | [`docs/roadmap.md`](roadmap.md) |
| Current state, blockers, next actions | [`docs/project-status.md`](project-status.md) |
| Risks | [`docs/risk-register.md`](risk-register.md) |
| Terms | [`docs/glossary.md`](glossary.md) |
| Decisions (ADRs) | [`docs/architecture/decisions.md`](architecture/decisions.md) |
| Subsystem specifications | [`docs/subsystems/`](subsystems/) and [`docs/ui/`](ui/README.md) |
| Firmware research | [`docs/firmware/`](firmware/independent-firmware-feasibility.md) |
| Device experiments, measurements, raw logs | [`docs/research/`](research/) (`logs/` holds raw device logs) |
| Understood but unscheduled work | [`docs/backlog.md`](backlog.md) |
| Templates | [`docs/templates/`](templates/) |
| Reference links and project concepts | [`docs/assets/references/`](assets/references/README.md), [`docs/assets/concepts/`](assets/concepts/README.md) |

Do not create empty documents to fill the structure. A new document needs a purpose that no
existing one covers; otherwise extend the existing one.

## 4. Keeping documentation true

Update the relevant documents in the same change as the code whenever any of these change:

- requirements or acceptance criteria → the subsystem spec (stable IDs, see §5);
- an architectural decision → a new ADR in `architecture/decisions.md` (never rewrite an accepted
  ADR; supersede it with a new one);
- an interface (a public `pw_` header, a file format, a button mapping, a memory layout) → the
  subsystem spec, and `CLAUDE.md` if it is something a session must know (commands, combos);
- implementation status → `docs/README.md` status table, `project-status.md`, `roadmap.md`;
- a device experiment → its research note (procedure, raw log in `research/logs/`, results,
  conclusions) and the status documents;
- a new risk or a retired one → `risk-register.md`.

Unresolved questions are written down (OPEN QUESTION / RESEARCH REQUIRED with the next action),
never guessed away. Decisions are recorded with their rationale and the alternatives rejected.
Do not replace or contradict an existing project rule without first understanding why it exists
(most rules in `CLAUDE.md` come from a failure on the device; the research notes say which).

## 5. Identifiers

| Kind | Format | Example |
|---|---|---|
| Requirement | `<AREA>-REQ-NNN` (functional) / `<AREA>-NFR-NNN` (non-functional) | `STE-REQ-004`, `UI-NFR-002` |
| Decision | `ADR-NNN` | `ADR-007` |
| Risk | `RISK-NNN` | `RISK-003` |
| Milestone | `M0`..`M7`, research gates `FW-G1`..`FW-G7`, documentation `DOC-n` | `M3` |
| Engine rungs / tests | `E1`..`E4`, `T0`..`T7` (E3 tests), `S1`..`S7` (speed backlog) | `T1` |

Areas: `SYS` core runtime, `STE` states engine, `STS` states storage, `SHL` in-game shell, `UI`
interface, `GFX` rendering and native assets, `CPL` complication framework, `DBG` debug tooling,
`BLD` build and verification, `VSH` XMB integration, `FW` firmware research.
IDs are never reused or renumbered; a dropped requirement is marked *Withdrawn*.

## 6. Writing a subsystem specification

Use [`templates/subsystem-spec.md`](templates/subsystem-spec.md). Sections A-H (overview,
requirements, architecture and integration, UX, references, implementation plan, testing and
acceptance, risks and decisions); mark a section *Not applicable* rather than padding it.
Requirements are testable (a number, a state, an observable behaviour), each with a priority
(P0-P3, see [roadmap](roadmap.md#priorities)) and a verification method (host test, `make check`,
device test with a named protocol, review). Link implementation files with relative paths and link
the research that justifies each design choice. Distinguish *current* from *target* behaviour in
every section.

Research notes use [`templates/research-note.md`](templates/research-note.md); decisions use
[`templates/decision-record.md`](templates/decision-record.md).

## 7. Research and device work

- Find the root cause before changing code for a device problem; no speculative test rounds
  (a device round costs the owner time). Prefer reading the firmware's behaviour (uOFW, PPSSPP,
  ARK-4, prior plugins) and the logs over trying fixes.
- Each device experiment has a written protocol (exact buttons, expected result, what to report),
  a raw log archived in `docs/research/logs/<date>-<what>.log`, and results written back into the
  research note the same day.
- Cite sources (URL, file and line where possible) next to the claims they support. Respect
  licenses: never copy code from PSPStates (no license) or intraFont (CC BY-SA); never put Sony
  files or screenshots of unclear license into the repository — link them instead.

## 8. UI and visual work

The UI specification ([`docs/ui/README.md`](ui/README.md)) is the reference for every screen.
Visual references (real products) live in `docs/assets/references/` as links with notes;
project concepts (our proposals) live in `docs/assets/concepts/` as editable SVG/HTML, each
labelled *PROPOSED*. Never present a concept or a reference as a screenshot of Pocketwatch.

## 9. Firmware feasibility work

The independent-firmware question is a research track with go/no-go gates
([`firmware/independent-firmware-feasibility.md`](firmware/independent-firmware-feasibility.md),
gates `FW-G1`..`FW-G7` in the [roadmap](roadmap.md)). It never blocks the plugin roadmap, and no
implementation starts before its gate is passed and recorded as an ADR. Anything that writes the
NAND, the IPL or the SYSCON is out of bounds without an explicit owner decision and a tested
recovery path.

## 10. Verification before finishing a task

- Code: `scripts\build.cmd`, `scripts\build.cmd -Target check`, host tests, format check (see
  `CLAUDE.md` for the exact commands).
- Documentation: `python tools/check_docs.py` (internal links resolve, requirement/ADR/risk IDs
  unique, every subsystem in the index has a spec, `CLAUDE.md` imports this file outside a code
  block). Fix what it reports.
- Instructions loaded: in a fresh session `/context` lists `CLAUDE.md` and `docs/AI_WORKFLOW.md`
  under Memory files (imports resolve relative to the importing file, are skipped inside code
  spans/blocks, nest at most four hops; project-internal imports need no approval).
- Status: `docs/README.md`, `project-status.md` and `roadmap.md` agree with each other and with
  the code.
- Report outcomes faithfully: what was verified on the device, what only builds, what is untested.

## 11. Commits

One commit per substantial finished chunk (code + tests + docs together), Conventional Commits,
no `Co-Authored-By` or other AI attribution lines, push only when the owner asks.
