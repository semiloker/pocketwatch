# M3a — taking over the HOME button

Question: how can HOME (PS) open Pocketwatch's hub instead of the system HOME screen, respect a
game that disabled HOME, and offer a native Quit Game?
Status: Answered by source research (2026-10-16); **device test pending**.

Labels: **VERIFIED** = read in source code (link), **INFERRED** = deduced from how working
plugins behave, **GUESS** = unconfirmed. Sources were read at pinned commits; no code was copied
(PSPStates has no license; the others are cited for behaviour only).

## 1. How the firmware handles HOME

- **impose.prx (`sceImpose_Driver`) owns the in-game HOME screen** (INFERRED: Custom Home and
  TempAR draw into impose's HOME framebuffer; impose itself is not reversed — uOFW has no impose
  module). Clues: PSPStates hooks impose's import of `sceCtrlPeekBufferPositive` to strip HOME
  ([main.c#L90](https://github.com/PSP-Archive/PspStates-Experiment/blob/deb401eda0c21176f5b2b8086d6f4393c3d4111d/pspstates_v2_src/src/pspstates_v2_src/main.c#L90));
  ARK/Adrenaline hook impose's `sceKernelWaitEventFlag` (bit 1 pause, bit 2 resume)
  ([impose_patch.c#L58](https://github.com/PSP-Archive/ARK-4/blob/7c03c3735503dc3fad999f4687b890fefef6467d/core/compat/pentazemin/impose_patch.c#L58)).
  So: impose reads the pad in kernel mode and signals its own thread (INFERRED).
- **Button masks do not stop it** (VERIFIED): masks/intercepts only filter the *user* buffers; the
  kernel buffer and the special-button callbacks see raw buttons
  ([uOFW ctrl.c#L1873](https://github.com/uofw/uofw/blob/28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9/src/kd/ctrl/ctrl.c#L1873),
  [#L1935](https://github.com/uofw/uofw/blob/28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9/src/kd/ctrl/ctrl.c#L1935),
  [#L1991](https://github.com/uofw/uofw/blob/28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9/src/kd/ctrl/ctrl.c#L1991)).
  Our `sceCtrlSetButtonMasks(0xFFFF, 1)` never blocks the system popup.
- **Bit 0x10000** is HOME in kernel reads (not filtered,
  [ctrl.c#L2140](https://github.com/uofw/uofw/blob/28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9/src/kd/ctrl/ctrl.c#L2140));
  in user mode it means "exit dialog visible"
  ([pspctrl.h#L80](https://github.com/pspdev/pspsdk/blob/9356a6481959977bac6515ee2d696fab96df41bc/src/ctrl/pspctrl.h#L80)).
  With HOLD on, the kernel buffer keeps only `0x3B0E0000`, so HOME is hidden (VERIFIED).
- **`sceImposeSetHomePopup(1/0)`**: 1 = HOME opens the system screen (default in a game), 0 = HOME
  shows nothing but stays readable from kernel mode; `sceImposeGetHomePopup` returns the flag
  (INFERRED from Custom Home, TempAR, GBAdhoc, TempGBA).
- **`sceImposeHomeButton(int)`**: undocumented. TempAR calls it with 0 to dismiss the screen
  around its menu ([menu.c#L64](https://github.com/raing3/tempar/blob/61cf50433f6d2aecad0de44dbf0f935d894946d9/src/objects/menu.c#L64));
  remotejoy-minus passes `sceImposeGetStatus() & 1` to inject a press
  ([main.c#L464](https://github.com/SourceK78/remotejoy-minus/blob/1e151fcc1f5924607e008a2c817f7dc681af549c/main.c#L464)).
  GUESS: 0 dismisses, other values open/toggle.

## 2. How existing plugins do it

| Plugin | Mechanism | Notes |
|---|---|---|
| Custom Home Mod ([main.c#L1774](https://github.com/PSP-Archive/Custom-Home-Mod/blob/1763e28defb15ba53c888c81c11235e35fa4cf3a/main.c#L1774)) | waits (≤ ~30 s) for `sceKernelCheckExitCallback() != 0`; gives up if HOME is held at start, the runlevel is not GAME, `GetHomePopup() == 0` or no exit callback; then `SetHomePopup(0)`, `SetUMDPopup(0)` and patches the function behind `sceImpose` 0x5595A71A / 0x72189C48 to return 0; polls `PSP_CTRL_HOME` every 5 ms from a kernel thread, acts on release | closes its menu on suspend and ignores input for 2 s after a resume ([common.c#L243](https://github.com/PSP-Archive/Custom-Home-Mod/blob/1763e28defb15ba53c888c81c11235e35fa4cf3a/common.c#L243)); does not respect a game's own disable |
| TempAR | `sceCtrlRegisterButtonCallback` (interrupt context, sets a flag), menu key HOME+R; on open `HomeButton(0)`, `SetHomePopup(0)`, 150 ms wait, masks `0xFFFF`=1 and `0x10000`=2 (game sees "dialog visible") | restores popup to 1 unconditionally |
| GBAdhoc home.prx ([main.c#L120](https://github.com/ShoshinFauteux/GBAdhoc/blob/205b7cc9113cb8dc6160fc91ab6330df757d1955/gpsp/psp/home/main.c#L120)) | saves `popup_orig`, never switches the popup off while HOME is held, counts a press only while the popup is off | restores on stop |
| MacroFire ([mfmain.c#L275](https://github.com/PSP-Archive/MacroFire/blob/8a9b51b90f32fb42a4bc807779cc86da9b194644/mfmain.c#L275)) | hooks `sceSysconSetDebugHandlers`, rewrites raw buttons for every reader | the only fully general way to hide HOME from impose |
| ARK-4 ([exitgame.c#L31](https://github.com/PSP-Archive/ARK-4/blob/7c03c3735503dc3fad999f4687b890fefef6467d/core/systemctrl/src/exitgame.c#L31)) | no HOME takeover; hooks sceCtrl_driver peek/read for L+R+START+DOWN (launcher) and L+R+SELECT+DOWN (XMB); refuses while `sceVshSDUtility_Module` or `sceDialogmain_Module` is loaded ([#L297](https://github.com/PSP-Archive/ARK-4/blob/7c03c3735503dc3fad999f4687b890fefef6467d/core/systemctrl/src/exitgame.c#L297)) | **pitfall:** our own kernel peeks go through ARK's hook, so that combo can reboot the PSP from our thread, even while we hold a state |

## 3. Quit Game

- `sceKernelInvokeExitCallback` takes a power lock (`sceKernelPowerRebootStart(0)`) and notifies the
  game's exit callback ([loadexec.c#L751](https://github.com/uofw/uofw/blob/28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9/src/kd/loadexec/loadexec.c#L751),
  [powerSwitch.c#L1938](https://github.com/uofw/uofw/blob/f7ed03aa7b507d34af9ca8c17ff3782e052e030c/src/kd/power/powerSwitch.c#L1938)),
  VERIFIED. The callback runs only when the game's callback thread waits with CB; with frozen
  threads, or a callback that never calls `sceKernelExitGame`, nothing happens and suspends stay
  blocked (INFERRED). The native "Yes" probably uses this path (GUESS).
- `sceKernelExitVSHVSH` must be called from kernel mode, not an interrupt, at user level 4
  ([loadexec.c#L635](https://github.com/uofw/uofw/blob/28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9/src/kd/loadexec/loadexec.c#L635));
  ARK patches loadexec's `GetUserLevel` to 4 ([loadexec.c#L22](https://github.com/PSP-Archive/ARK-4/blob/7c03c3735503dc3fad999f4687b890fefef6467d/core/systemctrl/src/loadexec.c#L22)).
  The reboot runs each user module's `module_reboot_before` in a new thread
  ([rebootModule.c#L16](https://github.com/uofw/uofw/blob/28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9/src/kd/modulemgr/rebootModule.c#L16))
  and then suspends all user threads itself
  ([loadexec.c#L1489](https://github.com/uofw/uofw/blob/28306bb5bd4fcdcc0c1d469bf1f3bdb885dde3e9/src/kd/loadexec/loadexec.c#L1489)), VERIFIED.
- Custom Home thaws the game, then `sceKernelExitVSHVSH(0)`
  ([main.c#L1433](https://github.com/PSP-Archive/Custom-Home-Mod/blob/1763e28defb15ba53c888c81c11235e35fa4cf3a/main.c#L1433)).
  ARK's own exit: `sceDisplaySetHoldMode(0)`, `SetUmdFile(NULL)`,
  `sctrlSESetBootConfFileIndex(MODE_UMD)`, then `sctrlKernelExitVSH` (clears `game_id`, calls the
  original ExitVSHVSH; [sctrl.c#L111](https://github.com/PSP-Archive/ARK-4/blob/7c03c3735503dc3fad999f4687b890fefef6467d/core/systemctrl/src/sctrl.c#L111)), VERIFIED.
- Safest order (GUESS, reasoned): thaw the game first (a `reboot_before` could deadlock on a frozen
  thread), then reboot from our kernel thread.

## 4. NIDs (ARK [nid_660_data.c](https://github.com/PSP-Archive/ARK-4/blob/7c03c3735503dc3fad999f4687b890fefef6467d/core/systemctrl/src/nid_660_data.c#L571))

| Library | Function | Old NID | 6.6x NID | In our table |
|---|---|---|---|---|
| sceImpose_driver | sceImposeSetHomePopup | 0x5595A71A | 0xC08C41EF | yes |
| sceImpose_driver | sceImposeGetHomePopup | 0x0F341BE4 | 0xE9A42056 | yes |
| sceImpose_driver | sceImposeHomeButton | 0x381BD9E7 | 0xEFB54A28 | yes |
| sceImpose_driver | sceImposeSetUMDPopup | 0x72189C48 | 0x9FF64142 | yes |
| sceImpose_driver | sceImposeGetStatus | 0x1B6E3400 | 0xB3EAE8F0 | yes |
| LoadExecForKernel | sceKernelExitVSHVSH | 0xA3D5E142 / 0x94A1C627 | 0x08F7166C | yes |
| LoadExecForKernel | sceKernelInvokeExitCallback | 0x62A27008 | 0x1F08547A | yes |
| LoadExecForKernel | sceKernelCheckExitCallback | 0x659188E1 | 0xB57D0DEC | yes |
| sceDisplay_driver | sceDisplaySetHoldMode | 0x7ED59BC4 | 0x3552AB11 | yes |
| sceCtrl_driver | sceCtrlRegisterButtonCallback | 0x5C56C779 | 0xDF53E160 | yes (`sceCtrl_driver_5C56C779`) |
| SystemCtrlForKernel | sctrlKernelExitVSH | 0x2794CCF4 | — (ARK library) | add to stubs |

The user library `sceImpose` keeps the old NIDs (reachable with `sctrlHENFindFunction`).

## 5. When the hub must not open

- while a utility dialog module is loaded (`sceVshSDUtility_Module`, `sceDialogmain_Module`;
  VERIFIED as ARK's check), probably also OSK/netconf (GUESS);
- while HOLD is on (automatic, VERIFIED);
- during a suspend and for ~2 s after a resume (Custom Home);
- when the game set the popup to 0 itself (INFERRED);
- while the engine is busy.

## 6. Recommended design (PROPOSED)

1. **Take over:** in the `UMD` runlevel, wait until `sceKernelCheckExitCallback() != 0`; read
   `orig = GetHomePopup()`; if 0, someone else owns HOME: keep the NOTE trigger and log it. Wait
   until HOME is up, `SetHomePopup(0)`, `HomeButton(0)`.
2. **Respect the game:** hook the function behind `sceImpose` 0x5595A71A so later calls by the game
   only update `game_wants_home` (and Get returns it); reapply 0 at RESUME_COMPLETED (GUESS: impose
   may restore it); put `orig` back in `module_stop`.
3. **Detect:** in the existing 20 ms kernel poll, HOME pressed → released opens the hub if the
   popup is ours, `game_wants_home`, no dialog module loaded, engine idle, ≥ 2 s since a resume.
   HOME inside the hub = Resume.
4. **Guard ARK's exit combos:** while the hub is open or a state is held, our peeks must not let
   L+R+START/SELECT+DOWN reboot underneath us (RESEARCH REQUIRED: read with a path ARK does not
   hook, or accept and discard the state first).
5. **Quit Game:** refuse while a dialog module is loaded; `pw_engine_discard`, seeds back, flush
   the log; clear masks, release the overlay, thaw; `sceDisplaySetHoldMode(0)`,
   `sctrlSESetUmdFile(NULL)`, `sctrlSESetBootConfFileIndex(MODE_UMD)`; `sctrlKernelExitVSH(NULL)`.
   Optional native-like path: `sceKernelInvokeExitCallback()` with a ~3 s fallback (it takes a
   power lock).

Device test (to write with the implementation): HOME opens the hub with no system popup flash;
HOME/O closes; Quit Game reaches the XMB; NOTE still mutes; a game that disables HOME (find one)
keeps it disabled; sleep with the hub open; HOME right after a resume.

## 7. Implementation and device test

Built 2026-10-16 (`shell/shell.c`, `ui/hub.c`, `ui/views.c`, `ui/dialog.c`, `gfx/pgf.c`,
`gfx/text.c`, `gfx/assets.c`), following §6 with two simplifications, both recorded as open:
no hook on the game's `sceImposeSetHomePopup` (a game that turns HOME off later is not detected;
instead the shell turns the system screen off again whenever it finds it back on, every 0.5 s),
and no `sceImposeHomeButton` call (its argument is not understood). Quit Game uses the game's exit
callback first and ARK's exit sequence after 5 s.

Device protocol (debug build, Castlevania; the PSP's `PSP/SAVEDATA` and `seplugins` were copied
to the PC first, `Desktop/psp-backup-2026-10-16`):

1. In gameplay press HOME once. Expected: no system "Quit the game?" screen; the game freezes,
   darkens over ~0.2 s, the hub shows a watch icon and Resume Game / Save State / Load State /
   Quit Game / Memory Stick Test in the system font.
2. Up/Down (the list slides), then Back (O on a Western PSP): the hub fades out, the game runs on
   and did not react to anything pressed.
3. HOME, Save State: "Saving...", a short pause, the hub again with "Saved at ...". HOME closes it.
4. Play ~20 s, HOME, Load State, Yes: black, then the game at the saved moment.
5. Hold NOTE: the system mute toggles, no hub.
6. HOME, Memory Stick Test, Yes (test T1, [m3-menu-e3](m3-menu-e3.md#t1-device-protocol)): note the
   message it ends with.
7. HOME, Quit Game, Yes: the XMB within ~5 s.
8. Connect USB: the log and the two test files are read on the PC.
