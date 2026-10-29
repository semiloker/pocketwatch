# M7 — where a game's △ menu in the XMB comes from

Question: on firmware 6.61, which module builds the options menu that △ opens on a game in
Game → Memory Stick, and how can Pocketwatch add "Continue from Save State" to it
([ADR-020](../architecture/decisions.md#adr-020--continue-from-a-state-through-the-games-options-menu),
variant B, confirmed by the owner 2026-10-29)?
Status: Open. The device probe (test X2) is built; it is waiting for its run.

## Background and sources

- **The XMB's own menus.** `vsh_module` shows a column item's △ menu from that item's list of
  `SceContextItem` entries (text key, sound, action, action argument) through its function
  `OnXmbContextMenu(arg0, arg1)`. ARK-4's xmbctrl finds that function 24 bytes before the word
  `0x00021202` and replaces its entry in vsh_module's function table (the first word equal to it
  above text + 0x50000) with its own, which clears the context of xmbctrl's items. **VERIFIED**
  (source): ARK-4 `extras/modules/xmbctrl/main.c` `PatchVshMain`, `OnXmbContextMenuPatched`
  (GPL-3.0).
- **Game Categories Lite** (Bubbletune, Codestation, GPL-3.0) uses the same mechanism to give the
  Game column's *Memory Stick item* its own △-style menu of categories: it builds a
  `SceContextItem` array, sets it as the item's `context` and calls `OnXmbContextMenu` itself; on
  6.60 the function is at vsh_module + `0x16468` and OnXmbPush at `+0x169B4`. It also hooks
  game_plugin_module's file calls (`sceIoDopen` / `Dread` / `Dclose` stubs at `+0x2A894..`) to
  filter the list, and knows the list's base path (`+0x215B4`). **VERIFIED** (source):
  [codestation/gclite](https://github.com/codestation/gclite) `context.c`, `gcpatches.c`.
  Neither project adds an entry to the △ menu of a *single game*.
- **The game list itself** belongs to `game_plugin_module`, which reads `ms0:/ISO`, `PSP/GAME`
  and the rest (Game Categories hooks exactly those reads). ARK-4's vshctrl also patches it, for
  homebrew and PS1 checks only. **VERIFIED** (source): ARK-4 `core/vshctrl/vshpatch.c`
  `patch_game_plugin_module`.
- **Open:** whether a game's △ menu (Information, Delete, ...) is a list of `SceContextItem`s that
  game_plugin hands to vsh_module's `OnXmbContextMenu`, or a menu game_plugin draws itself.
  - **If it goes through vsh_module:** "Continue from Save State" is one more entry added in a
    wrapper of `OnXmbContextMenu`, which pocketwatch_xmb.prx can install the way it already wraps
    xmbctrl's patches (ADR-021, device-verified for the Settings items). The entry's action leads
    to the slot list, and the launch is as vshctrl's `umdLoadExec`
    ([vsh-integration](../subsystems/vsh-integration.md#c-research-2026-10-17)).
  - **If game_plugin draws it:** the hook goes into game_plugin_module, located from a
    disassembly of the module as it runs (RESEARCH REQUIRED).
  - **Either way:** which structure holds the chosen game's path (`ms0:/ISO/...` or the EBOOT's
    folder), because the launch and the state's fingerprint need it. RESEARCH REQUIRED.
- **No public documentation** of game_plugin_module's menus was found (web search 2026-10-29:
  [GameBrew: Game Categories Revised](https://www.gamebrew.org/wiki/Game_Categories_Revised_PSP),
  [GameBrew: XMB Icon Manager](https://gamebrew.org/wiki/XMB_Icon_Manager_PSP) - leads only).

## Procedure

**The probe** ([`tools/xmbprobe/`](../../tools/xmbprobe/main.c), a research tool, not part of
Pocketwatch or any release): a user-mode VSH plugin that

1. finds `OnXmbContextMenu` and `OnXmbPush` the way xmbctrl does, locates their entries in
   vsh_module's function table before the older start-module handlers run, and wraps whatever
   those entries hold after xmbctrl and pocketwatch_xmb have patched them;
2. logs each call: the caller's return address, both arguments, 16 words at each and at the
   first word of the second (the first 40 calls in full), to `probe/probe.log`;
3. dumps `vsh_module` and `game_plugin_module` as they start: each segment
   (`<module>.seg<n>.bin`), the module record and the import and export tables with their NIDs
   (`<module>.txt`), for disassembly at the right addresses.

Build: `make -C tools/xmbprobe` (WSL), `make -C tools/xmbprobe check`. Install:
`scripts\deploy.cmd -Probe` copies it to `seplugins/pocketwatch/` and adds
`VSH, ms0:/seplugins/pocketwatch/xmbprobe.prx, on` after pocketwatch_xmb's line;
`scripts\deploy.cmd -ProbeOff` turns the line off after the run. Device protocol: test
[X2](../testing.md#pocketwatch-in-the-xmbs-settings-vsh-req-006).

**Safety.** The probe writes only to `seplugins/pocketwatch/probe/` and changes two function
pointers in vsh_module that xmbctrl already changes; its wrappers call through. If the XMB fails
to start, hold START at power-on (ARK-4 loads no plugins) and turn the line off. **The dumps are
Sony's code:** they stay on the owner's card and computer and are sent to the developer directly,
never committed and never attached to a public issue (AI_WORKFLOW section 7). Only addresses,
offsets and our own notes about them go into this file.

## Results

None yet.

## Conclusions

None yet. Next actions: the owner runs X2 and sends the `probe` folder. Then (1) the log answers
which module owns a game's △ menu; (2) the dumps, disassembled with `psp-objdump` at their logged
addresses, give the call site to hook and where the chosen game's path is; (3) an ADR records the
design before any Pocketwatch code changes, with the room it needs in pocketwatch_xmb.prx
(RISK-022: 295 bytes free on 2026-10-29).
