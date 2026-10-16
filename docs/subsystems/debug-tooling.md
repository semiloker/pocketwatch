# Debug tooling — specification

| | |
|---|---|
| Area code | `DBG` |
| Status | Done |
| Code | [`debug/probe.c`](../../plugin/src/debug/probe.c), [`debug/bench.c`](../../plugin/src/debug/bench.c); diagnostics inside [`engine.c`](../../plugin/src/complications/states/engine.c) |
| Research | every M1/M2 note; raw logs in [`research/logs/`](../research/logs/) |

## A. Overview

- **Purpose:** drive and observe the engine on the device without a UI: button combos for every
  experiment, memory/module/thread dumps, a throughput benchmark, and detailed logs (phase timings,
  suspend traces, breadcrumbs, post-mortem).
- **Non-goals:** anything in release builds; user-facing features.

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `DBG-REQ-001` | Debug builds (`DEBUG=1`) offer the combos listed below; release builds contain none of them. | P1 | Done | release build without `probe.o`/`bench.o` |
| `DBG-REQ-002` | Every combo press is logged; one action per press (wait for release). | P1 | Done | device logs |
| `DBG-REQ-003` | The dump combo drops a held state before writing to the card. | P0 | Done | code |
| `DBG-REQ-004` | The probe ignores combos while the hub is open. | P0 | Done | code |
| `DBG-REQ-005` | Each save/load logs per-phase timings, the slowest suspend events, breadcrumbs, and on failure the trace. | P1 | Done | device logs |

| Combo (debug builds, in a game) | Action |
|---|---|
| L+R+START+RIGHT / LEFT | save to RAM / load it |
| L+R+START+UP | throughput benchmark on the frozen game's RAM (no suspend) |
| L+R+SELECT+UP | drop the state, write the held log lines, dump memory/modules/threads |
| L+R+SELECT+RIGHT | rollback self-test |
| L+R+SELECT+LEFT | frozen save + immediate load |
| L+R+SELECT+TRIANGLE | one touch-and-go suspend (no real sleep) |
| L+R+SELECT+SQUARE | toggle fast mode (touch-and-go) |

L+R+START+DOWN and L+R+SELECT+DOWN are ARK-4's own exit combos and must stay unused.

## C. Architecture and integration

A kernel thread polls the pad; each combo calls an engine or bench entry point. The engine's
diagnostics are compiled in both profiles but heavy checks (`DIAG`) are off; `VERIFY` (thread
liveness for 5 s after a load) is debug-only.

## D-E. UX, references

Not applicable.

## F. Implementation plan

Keep the combos in sync with `CLAUDE.md`; move engine diagnostics to `states/diag.c`
(system map finding A5); when the hub has a debug page, mirror the experiments there.

## G. Testing and acceptance

Release build check (`DEBUG=0` builds and passes `make check`, 90.6 KB on 2026-10-16).

## H. Risks and decisions

The probe and the hub both drive the engine: engine owner lock planned (`STE-REQ-011`).
