# PSP XMB overhaul — branch overview

Branch `feature/psp-xmb-overhaul`, started 2026-10-17 from `main` at `914350a` (clean working tree;
`main` had just received `c710530`, the fix of loads from an earlier boot). Not committed by
request: the owner's brief for this branch says "do not commit or push automatically".

The brief asked for a complete redesign of the XMB with three styles, customization, a Save Data
Center, battery intelligence, per-game statistics and more. This page records what the project
actually is, what of that the platform allows, what this branch implements, and what comes next.
Labels as in [AI_WORKFLOW](../AI_WORKFLOW.md#2-evidence-labels).

## 1. Audit: what Pocketwatch is

| Fact | Evidence |
|---|---|
| Two PSP modules, not a homebrew application: `pocketwatch.prx` (kernel mode, UMD runlevel: loaded into retail games and ISOs only) and `pocketwatch_xmb.prx` (user mode, VSH runlevel: the XMB). | VERIFIED: `plugin/Makefile`, `vsh/Makefile`, `seplugins/PLUGINS.TXT` on the owner's card |
| Target: PSP-3000 with ARK-4 on 6.61 only. | VERIFIED: [project overview](../00-project-overview.md), probe log `model=3` |
| The XMB itself is Sony's (`vsh_module`, `paf`, `sysconf_plugin`). Pocketwatch does not draw the XMB; its XMB plugin adds an item and fills a native Settings page by patching call sites, the way ARK-4's xmbctrl does. | VERIFIED: [vsh-integration §C2](../subsystems/vsh-integration.md#c2-the-settings-entry-2026-10-17-vsh-req-006), ADR-021 |
| What Pocketwatch draws itself is the in-game hub over the frozen game: its screens exist in three styles - Classic (XMB look), Modern, Minimal - chosen in Settings. | VERIFIED: [styles](../ui/styles.md), ADR-019 |
| Features are complications: a folder and one table line each; the hub knows none of them. | VERIFIED: [complications](../subsystems/complications.md) |
| Settings live in `config.txt` / `language.txt`, read by both modules. | VERIFIED: `core/config.c`, `vsh/src/xsettings.c` |

## 2. What the platform allows (the brief's features, classified)

| Feature of the brief | Status | Where / why |
|---|---|---|
| Three coherent styles | IMPLEMENTED (hub, every screen incl. the new pages) | ADR-019; the same data for all three, presentation only |
| Redesign of the XMB's own navigation and look | BLOCKED BY PLATFORM (as a plugin) | Sony's renderer and layout; a plugin can add items and pages (done) but not replace the shell. A replacement shell is a firmware-level project: [FW track](../firmware/independent-firmware-feasibility.md). The XMB's look is themable with CTF themes (cxmb, already on the owner's PSP): NOT APPLICABLE here |
| Settings / customization (style, clock and battery, fast save, language) | IMPLEMENTED in the XMB: Settings → Pocketwatch (ADR-021, ADR-024) | accent colours, motion intensity, density: PROPOSED ([customization](../ui/customization.md)) |
| Theme packages | PROPOSED | needs validation rules first (brief §6); no external code ever |
| Save Data Center | PARTIALLY IMPLEMENTED | states are native saved data, browsed and deleted in the XMB's Saved Data Utility (M5); a richer browser with backups: PROPOSED - needs our renderer in the XMB (RESEARCH REQUIRED: drawing over `vsh_module` from the VSH plugin) or an in-game page per game |
| Battery dashboard and estimates | IMPLEMENTED in the XMB: Settings → Battery (ADR-022, ADR-024) | [battery](../subsystems/battery.md); syscon through ARK-4's kuKernelCall |
| Per-game play time and statistics | IMPLEMENTED: counted in the game, listed in the XMB: Game → Play Time (ADR-023, ADR-024) | [playtime](../subsystems/playtime.md); homebrew and PS1 games: NOT APPLICABLE (the game plugin does not load there) |
| Weekly / monthly summaries, session history | PROPOSED | needs a session log (PLT plan step 3) |
| Home dashboard, library search, favourites | PROPOSED | need our renderer in the XMB (as above) |
| System information page | PROPOSED | cheap in the hub (model, firmware, memory, card space) |
| Power profiles | RESEARCH REQUIRED | ARK-4 already sets CPU clocks per game/XMB (its Custom Firmware Settings): duplicating it would be a control that fights ARK's |
| Extension architecture | IMPLEMENTED (static) | complications; this branch added the background `tick` and the `on_quit` hooks (CPL-REQ-003). Dynamic third-party modules: not planned |
| Diagnostics | PARTIALLY IMPLEMENTED | `pocketwatch.log`, `xmb.log`; an in-hub page PROPOSED |
| Recovery | IMPLEMENTED | bad config falls back to defaults; a broken XMB plugin: hold START at boot (ARK-4 loads no plugins) |

## 3. Architecture of the additions

```
XMB (pocketwatch_xmb.prx, user mode)
  Settings -> Pocketwatch, Battery      Game -> Play Time        (items added by AddVshItem)
        \---------- System Settings' page with our rows (sysconf_plugin hooks) ----------/
  xsettings.c: rows of the open page  <- config.txt / language.txt / playtime-a|b.txt
  xbattery.c: thread, a reading every 15 s -> core/battmodel.c (portable)
                                    scePower (user) + syscon via kuKernelCall
Game (pocketwatch.prx, kernel mode)
  complications/playtime: tick (1 s) -> P9 live block -> playtime-a|b.txt (5 min, after the hub, Quit)
  complications/settings: config.txt at start
  hub: Resume Game, Save State, Load State, Quit Game
```

Shared portable code built into both: `core/config.c`, `core/playtime.c`, `ui/i18n.c` (the game
build without the XMB-only strings), `ui/langnames.c` (XMB only), `core/battmodel.c` (XMB only).

## 4. Performance budget

Kernel RAM is the scarce resource (RISK-005). With the pages in the XMB (ADR-024) the game build
is 175 KB loaded (release), under its old 176 KB budget; it adds only the play-time counter (P9)
and its tick. The XMB plugin is 45 KB (file) / 57 KB loaded of a 64 KB budget. Background work:
in the XMB one battery reading per 15 s; in a game one counter update per second and a card write
at most every 5 minutes of play. Measured on the PSP: the hub still opens in ~310 ms (log).

## 5. Status and next steps

| Phase (brief §15) | State |
|---|---|
| 1 Audit and branch | done (this page) |
| 2 Specification | done for BAT and PLT; the XMB-side features are classified above |
| 3-5 UI foundation, styles, customization | existed before the branch (ADR-019, ADR-021); info pages added |
| 6 Save Data Center | PROPOSED (see §2) |
| 7 Battery | IMPLEMENTED, device test B1 pending |
| 8 Statistics | IMPLEMENTED, device test P1 pending |
| 9-10 Extras, validation | host tests 99/99, `make check` both profiles; no device run yet |

Next: B1 and P1 on the PSP; then the system information page and a session log; then the
research for drawing our own pages in the XMB (needed by the Save Data Center, the dashboard and
the XMB battery and statistics pages).
