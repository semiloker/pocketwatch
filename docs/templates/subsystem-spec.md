# <Subsystem name> — specification

<!-- Copy this file to docs/subsystems/<name>.md (or docs/ui/ for interface work), replace every
<...>, delete guidance comments. Mark a section "Not applicable" instead of padding it.
Label claims VERIFIED / INFERRED / PROPOSED / OPEN QUESTION / RESEARCH REQUIRED
(docs/AI_WORKFLOW.md §2). Requirement IDs: <AREA>-REQ-NNN / <AREA>-NFR-NNN, never reused. -->

| | |
|---|---|
| Area code | `<AREA>` |
| Status | Planned / In progress / Done (per part, see A) |
| Milestones | <M?> |
| Code | <relative links to the implementation files> |
| Research | <relative links to research notes> |

## A. Overview

- **Purpose and responsibilities:** <what it owns, in two or three sentences>
- **Problems solved / user benefit:** <...>
- **Scope:** <...>
- **Non-goals:** <what it deliberately does not do>
- **Current state:** <what exists and is verified, with evidence>
- **Target state:** <what it must become; planned maturity>

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `<AREA>-REQ-001` | <testable statement> | P0 | Done / Planned | <host test / make check / device protocol / review> |
| `<AREA>-NFR-001` | <performance, reliability, resource, security limit with a number> | P1 | | |

Dependencies and compatibility requirements: <...>

## C. Architecture and integration

- Internal components and their responsibilities.
- Connections to other subsystems (link their specs) and the interfaces used (headers, events,
  file formats, hardware registers).
- Data flow, control flow, lifecycle; the execution context of each part (thread, sysevent
  handler, suspend phase).
- Configuration points and extension mechanisms.
- Ownership of persistent state and shared resources (memory partitions, files, display, buttons).
- Failure handling, recovery, fallback.

<!-- A mermaid diagram where it helps. -->

## D. User experience and visual design

<!-- Not applicable for internal services. Otherwise: appearance, screens and states (loading,
empty, disabled, error, recovery), navigation, customization, accessibility. Link docs/ui/. -->

## E. Reference material

| Reference | Source | What to learn | Adopt / adapt / avoid | License |
|---|---|---|---|---|

## F. Implementation plan

- Files and components involved; proposed module boundaries and interfaces.
- Steps and their dependencies.
- Alternatives and trade-offs.
- Migration and compatibility (state formats, settings).
- Blockers and open architectural questions.

## G. Testing and acceptance

| Requirement | Test | Acceptance criterion | Needs |
|---|---|---|---|

Include edge cases, failure and recovery tests, resource/performance measurements.

## H. Risks and decisions

- Risks (link `RISK-NNN` in docs/risk-register.md) and mitigations.
- Decisions (link `ADR-NNN` in docs/architecture/decisions.md) and alternatives considered.
- Open questions with the next research action.
