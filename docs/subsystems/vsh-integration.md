# XMB integration — specification

| | |
|---|---|
| Area code | `VSH` |
| Status | Planned (M7); depends on [states storage](states-storage.md) |
| Code | — (future separate VSH-runlevel PRX under `vsh/`) |

## A. Overview

- **Purpose:** continue a game from a state straight from the XMB, so a state behaves like a
  native "resume" point.
- **Current state:** none. States already appear natively in Game → Saved Data Utility once M5
  writes SAVEDATA folders (no VSH code needed for browsing and deleting, **VERIFIED** by S2b).
- **Target state:** a "Save States" entry or a per-game option that launches the ISO and loads the
  chosen state after the first frames.

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `VSH-REQ-001` | From the XMB the user can pick a state of a game and start the game from it. | P2 | Planned | device test |
| `VSH-REQ-002` | The game is launched the same way the XMB launches it (same path, same ARK boot config), so the E3 fingerprint matches. | P2 | Planned | device test |
| `VSH-REQ-003` | The GAME-side plugin loads the requested state automatically once the game runs; a refusal falls back to a normal boot with a message. | P2 | Planned | device test |
| `VSH-REQ-004` | XMB icons and texts come from the firmware's own resources ([rendering](../ui/rendering.md)). | P2 | Planned | owner review |

## C. Architecture and integration

PROPOSED: a VSH PRX adds the entry (ARK-4 xmbctrl-style hooks, GPL-3.0) and passes the slot to the
GAME plugin through a file or ARK's reboot config; the GAME plugin waits for the first stable
frames, then runs a normal E3 load.

## D. User experience and visual design

Native XMB item with the state's ICON0/PIC1; options menu (△) like the Saved Data Utility.

## E. Reference material

ARK-4 xmbctrl ([ARK-4](https://github.com/PSP-Archive/ARK-4), GPL-3.0): adding XMB items.

## F. Implementation plan

After M6. RESEARCH REQUIRED: how to launch an ISO from a VSH plugin with the same boot config as the
XMB; how to hand the slot to the GAME runlevel.

## G-H. Testing, risks

To be written when M7 starts. Depends on [RISK-006](../risk-register.md#risk-006--cross-boot-load-into-a-different-environment).
