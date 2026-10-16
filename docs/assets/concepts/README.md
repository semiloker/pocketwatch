# UI concepts (PROPOSED)

Pocketwatch's own design proposals. Every file here is **PROPOSED** and is **not a screenshot of
Pocketwatch or of the PSP**: shapes, icons and fonts are placeholders drawn for discussion
(the real build reads the PSP's own fonts and icons at runtime). Geometry follows the metrics
collected in [m4-native-look](../../research/m4-native-look.md), most of them INFERRED until a
device capture confirms them. Open the SVGs in a browser; they are hand-editable.

| Concept | File | Shows | Requirements |
|---|---|---|---|
| A — item column hub (recommended for v0.1) | [hub-a-column.svg](hub-a-column.svg) | the XMB's vertical list over the frozen, dimmed game; category icon, selected row with description, following rows, optional clock/battery | UI-REQ-001, UI-REQ-002, UI-REQ-006, UI-REQ-007, UI-REQ-012 |
| B — cross hub | [hub-b-cross.svg](hub-b-cross.svg) | complications as XMB categories (Game, States, Settings) | UI-REQ-002, CPL-REQ-002 |
| State list | [state-list.svg](state-list.svg) | Load/Save State in the Saved Data Utility layout with the state's screenshot as background | UI-REQ-009, STS-REQ-011 |
| Confirmation dialog | [dialog-confirm.svg](dialog-confirm.svg) | system message-dialog style, Yes/No, hint bar | UI-REQ-010, UI-REQ-011 |

## How they differ from the current implementation

The phase-1 menu (device-verified 2026-10-16) uses the built-in 8x8 font at 2x, a black band
across the middle, a white bar for the selection, a status line and an "X select O back" hint; it
opens on a NOTE tap and lists Save state / Load state / Resume game. The concepts replace all of
that with the system's vocabulary; the behaviour (frozen game, masked buttons, held-freeze engine
calls, display hand-off around suspends) stays.

## Decisions needed from the owner

- Concept A or B for v0.1 ([UI spec §H](../../ui/README.md#h-risks-and-decisions)).
- Whether "Pocketwatch" appears in the hub at all.
