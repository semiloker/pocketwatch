# XMB integration — specification

| | |
|---|---|
| Area code | `VSH` |
| Status | In progress (M7): variant B chosen ([ADR-020](../architecture/decisions.md#adr-020--continue-from-a-state-through-the-games-options-menu)); the GAME side (VSH-REQ-003) implemented 2026-10-17, device test V1 pending; the XMB side not started; depends on the cross-boot load (T3/T6, not yet run on the device) |
| Code | GAME side: `continue.txt` in [`states_ui.c`](../../plugin/src/complications/states/states_ui.c), the start-up hook in [`shell.c`](../../plugin/src/shell/shell.c); XMB side: — (a VSH-runlevel PRX under `vsh/`) |

## A. Overview

- **Purpose:** continue a game from a state straight from the XMB, so a state behaves like a
  native "resume" point.
- **Current state:** states already appear natively in Game → Saved Data Utility (M5): browse,
  see the picture, delete. Continuing needs the game started and HOME → Load State.
- **Target state:** pick a state from the XMB and the game starts in that moment.

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `VSH-REQ-001` | From the XMB the user can pick a state of a game and start the game from it. | P2 | Planned | device test |
| `VSH-REQ-002` | The game is launched the same way the XMB launches it (same path, same ARK boot config), so the E3 fingerprint matches. | P2 | Planned | device test |
| `VSH-REQ-003` | The GAME-side plugin loads the requested state automatically once the game runs; a refusal falls back to a normal boot with a message. | P2 | Implemented (device test pending): `continue.txt` (below), the complications' start-up hook | testing guide V1, `registry_finds_a_pending_startup` |
| `VSH-REQ-004` | XMB icons and texts come from the firmware's own resources ([rendering](../ui/rendering.md)). | P2 | Planned | owner review |
| `VSH-REQ-005` | Nothing breaks ARK-4's own XMB additions (xmbctrl's "Custom Firmware Settings" and its plugin manager). | P2 | Planned | device test with ARK's menus |

## C. Research (2026-10-17)

**Launching an ISO the XMB's way** (VERIFIED, ARK-4 `core/vshctrl/vshpatch.c`, `umdLoadExec`):
vshctrl hooks the XMB's `sceVshBridge` launch imports (NIDs `0x63E69956`, `0x0C0D5913`) and,
for an ISO on a PSP-3000, sets `sctrlSESetBootConfFileIndex(MODE_UMD)` /
`sctrlSESetUmdFile` and calls `sctrlKernelLoadExecVSHWithApitype(0x120, "disc0:/PSP_GAME/SYSDIR/EBOOT.BIN", param)`
(Inferno mounts the ISO; apitype 0x123/0x125 are the PSP Go's ms0/ef0 paths). A VSH plugin can
repeat exactly that with the ISO path from the state's STATE.BIN, which keeps the fingerprint
(path, size) identical.

**Adding XMB items** (VERIFIED, ARK-4 `extras/modules/xmbctrl/main.c`, GPL-3.0 like ours):
xmbctrl scans `vsh_module`'s code for instruction patterns and patches the calls to
`AddVshItem` (insert items into a category), `ExecuteAction` (what an item does),
`OnXmbPush` / `OnXmbContextMenu` (sub-menus, the △ menu) and `scePafGetText` (the items' texts).
The patterns are 6.61-specific; ARK's own xmbctrl already holds those call sites on the owner's
PSP, so a second patcher must find the already-patched call and chain to it (feasible: the call
target can be read back from the instruction, `U_EXTRACT_CALL`), and must load after it.

**Handing the slot to the game** (implemented, GAME side): a request file next to the PRX,
`seplugins/pocketwatch/continue.txt`, one line `<disc ID> <slot 1-5>` (`ULUS10277 2`; a dash in
the ID is ignored). The XMB side will write it before the launch. The GAME side reads and deletes
it at plugin start (it acts once, even if the game crashes) and ignores it for another game.
Once the game runs (its exit callback registered, as for the HOME takeover, or the 30 s timeout)
and 1.5 s later (INFERRED: games load their own modules right after registering the callback),
the shell freezes the game, opens the screen and calls the States complication's `startup`: the
same path as Load State (guards, store load with progress, `pw_store_arm`, rollback, marker
check), without the confirmation. A guard or a failed load shows its message and the game goes on
from its start. While a system dialog is up or the PSP has just resumed it retries for up to 30 s,
then drops the request (logged).

## D. Variants (owner chose B, 2026-10-17)

| Variant | What the user does | Work and risk |
|---|---|---|
| **A. "Save States" item in the Game category** | XMB → Game → Save States → a list of every game's states (our state list, drawn by us over the XMB) → Enter starts the game in that state | new VSH PRX: item injection chained after ARK's xmbctrl, a renderer in the VSH, the launch; highest effort, the most native |
| **B. △ on a game's icon → "Continue from Save State"** | the game's own options menu gets an entry; then a slot list | the same hooks (context menu); needs the selected game's path from the XMB's item; less screen work than A |
| **C. No XMB code: a prompt at the game's start** | start the game as usual; when it has states, Pocketwatch asks over its first frames "Continue from Save State N?" (✕/○) | only the GAME side; lowest risk; not inside the XMB |

The GAME-side start-up load (VSH-REQ-003) is shared by all three.

## E. Reference material

| Reference | Source | What to learn | Adopt / adapt / avoid | License |
|---|---|---|---|---|
| ARK-4 vshctrl | `core/vshctrl/vshpatch.c` | the ISO launch path and boot config | adopt (same calls) | GPL-3.0 |
| ARK-4 xmbctrl | `extras/modules/xmbctrl/main.c` | item injection, actions, texts | adapt, chained after it | GPL-3.0 |

## F. Implementation plan

1. Owner: run T3/T6 (load after a restart / power-off): M7 is only useful when they pass.
2. ~~Owner: choose A, B or C~~ — B ([ADR-020](../architecture/decisions.md#adr-020--continue-from-a-state-through-the-games-options-menu)).
3. ~~GAME side: the start-up load from a request~~ — done; owner: V1.
4. VSH side (RESEARCH REQUIRED first): the XMB's △ menu call sites in 6.61's `vsh_module` and
   how xmbctrl already patches them; how the context menu knows the selected game's path; then a
   VSH PRX with chained hooks, the slot list and the launch; device tests with ARK's menus.

## G-H. Testing, risks

Device tests per variant, written with the implementation. Risks:
[RISK-006](../risk-register.md#risk-006--cross-boot-load-into-a-different-environment); for A/B a
VSH crash on boot (recovery: ARK's recovery menu / disabling the plugin in PLUGINS.TXT from a PC).
