# Pocketwatch documentation

This directory is the project's source of truth for approved requirements, architecture,
decisions, status and research. How to work with it: [AI_WORKFLOW.md](AI_WORKFLOW.md) (imported by
the root `CLAUDE.md`). Current state and next actions: [project-status.md](project-status.md).

Evidence labels used everywhere: **VERIFIED**, **INFERRED**, **PROPOSED**, **OPEN QUESTION**,
**RESEARCH REQUIRED** ([definitions](AI_WORKFLOW.md#2-evidence-labels)).

## Start here

| Document | Purpose |
|---|---|
| [00-project-overview.md](00-project-overview.md) | vision, owner's priorities, scope, non-goals, v0.1 success criteria |
| [01-system-architecture.md](01-system-architecture.md) | layers, execution contexts, memory ownership, key flows, dependencies |
| [02-system-map.md](02-system-map.md) | subsystem inventory, interaction map, shared resources, architecture review |
| [roadmap.md](roadmap.md) | milestones, priorities P0-P3, acceptance criteria, firmware research gates |
| [project-status.md](project-status.md) | what is done, in progress, blocked; next actions |
| [risk-register.md](risk-register.md) | risks RISK-001.. with mitigations |
| [architecture/decisions.md](architecture/decisions.md) | ADR-001.. architecture decisions |
| [glossary.md](glossary.md) | terms and abbreviations |
| [backlog.md](backlog.md) | understood but unscheduled work (speed S1-S7) |
| [testing.md](testing.md) | step-by-step device tests for every feature, what to send back |
| [compat.md](compat.md) | the owner's 22 games and how States behaves in each (v0.1.0 gate) |

## Subsystems

| Area | Subsystem | Status (2026-10-16) | Specification |
|---|---|---|---|
| `SYS` | Core runtime | Done (config, game identity, write gate Planned) | [subsystems/core-runtime.md](subsystems/core-runtime.md) |
| `STE` | States engine | Done (E1, E2, fast mode); E4 Planned | [subsystems/states-engine.md](subsystems/states-engine.md) |
| `STS` | States storage (E3, M5) | Implemented, device tests pending | [subsystems/states-storage.md](subsystems/states-storage.md) |
| `SHL` | In-game shell (HOME, guards, Quit Game) | Partial | [subsystems/game-shell.md](subsystems/game-shell.md) |
| `UI` | User interface | Partial (phase-1 menu) | [ui/README.md](ui/README.md) |
| `GFX` | Rendering and native assets | Partial | [ui/rendering.md](ui/rendering.md) |
| `CPL` | Complication framework | Implemented (registry, System, States, Settings) | [subsystems/complications.md](subsystems/complications.md) |
| `DBG` | Debug tooling | Done | [subsystems/debug-tooling.md](subsystems/debug-tooling.md) |
| `BLD` | Build and verification | Done | [subsystems/build-and-verification.md](subsystems/build-and-verification.md) |
| `VSH` | XMB integration | Planned (M7) | [subsystems/vsh-integration.md](subsystems/vsh-integration.md) |
| `FW` | Independent firmware feasibility | Research | [firmware/independent-firmware-feasibility.md](firmware/independent-firmware-feasibility.md) |

UI companions: [visual references](ui/references.md), [customization architecture](ui/customization.md),
[concepts (PROPOSED mockups)](assets/concepts/README.md), [reference image policy](assets/references/README.md).

## Research notes

| Note | Question | Status |
|---|---|---|
| [research/m1-hardware.md](research/m1-hardware.md) | memory map in a retail game; does the XMB list our saves | Answered |
| [research/m2-engine.md](research/m2-engine.md) | E1 capture, E2 same-boot load | Answered (E2 done) |
| [research/m2-speed.md](research/m2-speed.md) | making save/load fast | Answered for now (backlog S1-S7) |
| [research/m3-menu-e3.md](research/m3-menu-e3.md) | minimal menu; E3 design | Phase 1 done; E3 open |
| [research/m3-home-takeover.md](research/m3-home-takeover.md) | HOME instead of the system HOME screen; Quit Game | Answered by sources; device test pending |
| [research/m4-native-look.md](research/m4-native-look.md) | PGF fonts, XMB metrics, buttons, language | Answered by sources; device check pending |
| [research/m4-native-icons.md](research/m4-native-icons.md) | XMB icons from RCO/GIM | Answered by sources; device check pending |
| [research/logs/](research/logs/) | raw device logs, one per run | — |

## Templates

[subsystem spec](templates/subsystem-spec.md) · [research note](templates/research-note.md) ·
[decision record](templates/decision-record.md)

## Checking the documentation

```sh
python tools/check_docs.py
```

It verifies internal links and anchors, requirement/ADR/risk identifiers, that every spec is
linked from this index, and that `CLAUDE.md` imports `docs/AI_WORKFLOW.md`.
