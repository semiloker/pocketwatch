# M7 — Pocketwatch's keyboard inside games (instead of the firmware's OSK)

Question: how can a game that opens the PSP's on-screen keyboard (`sceUtilityOsk*`) get
Pocketwatch's keyboard instead (owner, 2026-10-20: "in games and in the menu"), and what does it
cost the game plugin?
Status: Open (2026-10-20) - research done from sources; the device probe (G1) is next.

## Background and sources

**How a game uses the keyboard.** The game fills `SceUtilityOskParams` (the dialog's common part,
`datacount`, a pointer to `SceUtilityOskData`) and calls `sceUtilityOskInitStart`; then, every
frame after its own drawing, `sceUtilityOskGetStatus` and, while the status is VISIBLE,
`sceUtilityOskUpdate(n)`; when the status is QUIT it calls `sceUtilityOskShutdownStart` and waits
for FINISHED / NONE. Home did exactly this until ADR-039 (`home/src/dialogs.c`: `hm_dialog_osk` with
`hm_dialog_run`, before `b887ab6`). **VERIFIED** (PSPSDK `psputility.h`: NONE 0, INIT 1, VISIBLE 2, QUIT 3, FINISHED
4; `psputility_osk.h:154`).

**The NIDs** (user library `sceUtility`, not randomized): `sceUtilityOskInitStart` 0xF6269B82,
`sceUtilityOskShutdownStart` 0x3DFAEBA9, `sceUtilityOskUpdate` 0x4B85C861,
`sceUtilityOskGetStatus` 0xF3F76017. **VERIFIED** (the `.rodata.sceNid` words of
`libpsputility.a`, objects `sceUtility_0025..0028.o`, pspdev 2026-10-20).

**What the dialog reads and writes** (`SceUtilityOskData`, `psputility_osk.h:94-123`):
`language`, `inputtype` (flags: Latin digits / signs / lower / upper, Japanese kinds, Russian
lower / upper, Korean, URL; 0 = all), `lines`, `desc` (the title, UCS-2), `intext` (the text to
start from), `outtext` + `outtextlength` (the buffer, UCS-2), `outtextlimit` (characters at most),
`result` (UNCHANGED 0, CANCELLED 1, CHANGED 2). PPSSPP's implementation reads `desc`, `intext`,
`outtextlimit` (the smaller of it and `outtextlength - 1`, or the latter when 0) and `inputtype`;
it writes `outtext` (zero-filled to the end), `fields[0].result` and `base.result = 0`; Init
refuses a dialog that is not NONE (`SCE_ERROR_UTILITY_INVALID_STATUS`) and checks the
parameter size (0x40 or 0x44); Update is refused unless the dialog is running; Shutdown is
refused before FINISHED. Delays: 300 ms from Init to running, 40 ms for the shutdown.
**VERIFIED** (PPSSPP `Core/Dialog/PSPOskDialog.cpp`, `PSPDialog.cpp`, master 2026-10-20, GPL-2.0+:
read, not copied).

**Where the firmware draws it.** PPSSPP draws its dialogs with a GE list (`PPGeBegin`..`PPGeEnd`,
`sceGeListEnQueue` with a saved context) that sets no frame buffer: it draws onto whatever buffer
the game's GE state points at - the game's draw buffer, which the game then shows. **VERIFIED**
for PPSSPP (`Core/Util/PPGeDraw.cpp`); **INFERRED** for the PSP's own utility module (PPSSPP
emulates what games rely on).

**Replacing it.** ARK-4's SystemCtrl has `sctrlHENFindFunction(module, library, nid)` (the plugin
already uses it, `core/sysctrl.h`) and `sctrlHENPatchSyscall(addr, newaddr)`: every syscall to
the function then runs `newaddr` (pspdev `systemctrl.h:840-850`). **INFERRED**: the four OSK
exports can be redirected this way from the kernel plugin; RESEARCH REQUIRED: the NID of
`sctrlHENPatchSyscall` in ARK-4 6.61's `SystemCtrlForKernel` (`make check` must know it) and
that the `sceUtility_Driver` exports are syscalls the patch reaches.

**The budget.** The game plugin's loaded sections are 194 552 of 196 608 bytes (debug, `make
check` 2026-10-20, ADR-037); release 180 224 at most (RISK-005); P1 has ~300 KB free in games,
shared with other plugins and the game's drivers (RISK-005).

## Options

| | How | Cost | Risk |
|---|---|---|---|
| A | In the game plugin: the four functions redirected; Update runs Pocketwatch's keyboard modally over the frozen game (the hub's renderer, fonts and button reading), writes `outtext` and `result`, and reports QUIT; the keyboard's model (`home/src/keyboard.c`, portable) shared | ~5-6 KB of kernel RAM (INFERRED: the model ~2 KB, drawing ~3 KB, the hooks < 1 KB) - over the debug budget by ~4 KB | the budget (RISK-005); a game that keeps drawing while the keyboard is up sees a still frame |
| B | A second kernel PRX in `PLUGINS.TXT` for games with its own renderer | 40-60 KB of P1 all the time (a second copy of the fonts and drawing) | P1 shared with other plugins |
| C | A small hook in the game plugin loads a keyboard PRX on the game's first `sceUtilityOskInitStart` and unloads it after the shutdown; if it cannot load, the call goes on to the firmware | < 1 KB always, the PRX's size only while typing | the load from the card in the game's thread (a few hundred ms, PROPOSED to measure); the PRX must draw (fonts) on its own |

Common to all: the game's `inputtype` decides the pages (digits only: the digit row; Latin /
Russian flags: those pages); `outtextlimit` and `lines` are kept; the text goes back as UCS-2;
Back with changes asks Save / Discard / Keep Editing as in Home; anything unexpected (a second
field, `datacount` != 1, a size other than 0x40 / 0x44) goes on to the firmware's keyboard
untouched.

## Procedure

**G1 - which games use it (no change to the game).** A debug build whose hook only logs the call
(`osk: <disc ID> inputtype X lines N limit L language G desc "..."`) and calls the firmware's own
function. The owner opens a name or text entry in each game that shows the PSP's keyboard and
sends `pocketwatch.log`. Safety: nothing changes for the game; the log is deferred.

**G2 - the keyboard in one game** (option chosen after G1): the redirected functions in a debug
build, tried first in PPSSPP, then on the device in one game from G1: type, erase, Discard, Save;
the game must take the text (its name on screen) and go on.

## Results

None yet.

## Conclusions

- Possible from the sources: the game's contract is four functions and one parameter block; the
  PSP's dialogs draw on the game's own frame buffer (INFERRED), so a replacement may draw there
  too, or modally over the frozen game as the hub does.
- OPEN QUESTION (owner): option A (raise the debug budget by ~4 KB or trim elsewhere), B or C.
  Recommendation: C - nothing is paid in the many games that never open a keyboard, and the
  firmware's keyboard stays the fallback.
- Next: G1, after the owner names the games that open the PSP's keyboard.
