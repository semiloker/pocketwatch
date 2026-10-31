> **Pre-release.** Tested on one PSP-3000 with ARK-4 on 6.61 only. Back up your Memory Stick
> (at least `PSP/SAVEDATA`) before you use save states.

## What is in the zip

| Part | File on the Memory Stick | What it does |
|---|---|---|
| Game plugin | `seplugins/pocketwatch/pocketwatch.prx` | HOME in a game opens Pocketwatch's menu: Resume, Save State, Load State, Screenshot, Quit Game |
| XMB plugin | `seplugins/pocketwatch/pocketwatch_xmb.prx` | XMB → Settings → Pocketwatch and Battery, XMB → Game → Play Time, the start-screen choice |
| Pocketwatch Home | `PSP/GAME/PocketwatchHome/EBOOT.PBP` and `PSP/SAVEDATA/ARK_01234/PWHOME.PBP` | A home screen of its own: games, music, photos, settings, USB, Wi-Fi |
| Overlay (experimental) | `seplugins/pocketwatch/pocketwatch_perf.prx` | FPS and volume/brightness indicator in games; off until Home turns it on |
| Translations | `seplugins/pocketwatch/lang/*.txt` | The menus follow the system language (English is built in) |

## Install

**[The install guide](https://github.com/semiloker/pocketwatch/blob/main/INSTALL.md) has every step,
how to check it worked, and what to do when it does not.** It is also in the zip as `INSTALL.md`.
The short version, for a PSP already running ARK-4 on firmware 6.61:

1. Download the zip below under **Assets** and unzip it. Copy the `seplugins` and `PSP` folders from
   inside its folder to the root of the Memory Stick and merge them with the folders already there.
2. Add these two lines to `seplugins/PLUGINS.TXT`. On a PSP Go's internal storage, write `ef0:`
   instead of `ms0:` (untested).

   ```text
   UMD, ms0:/seplugins/pocketwatch/pocketwatch.prx, on
   VSH, ms0:/seplugins/pocketwatch/pocketwatch_xmb.prx, on
   ```

   Do not add a line for `pocketwatch_perf.prx`: Home → Settings → Pocketwatch → Overlay in Games
   writes it.
3. Turn the PSP off and on again. With a Live install of ARK-4, run ARK Loader again as well.
   Pocketwatch appears under XMB → Settings → Pocketwatch, and HOME in a game opens its menu.
4. Optional: to start the PSP in Pocketwatch Home instead of the XMB, open XMB → Settings →
   Pocketwatch → Start screen → Pocketwatch Home. You can also start Home from Game → Memory Stick.

**If something goes wrong at power-on:**

| Hold at power-on | Result |
|---|---|
| START | ARK-4 loads no plugins, so you can fix `PLUGINS.TXT` |
| L | The XMB starts this once, even when Pocketwatch Home is the start screen. On a permanent ARK-4 install with DC-ARK, L may start DC-ARK instead: use Home's Exit to the XMB. |

To uninstall, set the Start screen back to XMB, delete the two lines from `PLUGINS.TXT`, then the
`seplugins/pocketwatch` and `PSP/GAME/PocketwatchHome` folders and
`PSP/SAVEDATA/ARK_01234/PWHOME.PBP`.

## Known issues

- **Save states are experimental.** A state loads only on the same setup that made it: same
  Pocketwatch version, game file and PSP. States from an older version are refused. Loading after
  a power-off has not been fully tested on the device yet.
- The 3rd Birthday rebooted the PSP during a save once.
- Do not use Pocketwatch together with another savestate plugin, such as PSPStates or Custom
  Home's states.
- The in-game overlay is experimental. Leave it off unless you are testing it: an earlier version
  turned the PSP off on Quit Game.
- After USB Connection, Pocketwatch Home restarts by design. This keeps the Memory Stick's files
  safe.
- The Internet Browser in Home fails to open (error `0x800200D9`).
- ARK-4's L+R+START+DOWN combo turned the PSP off once instead of opening ARK-4's menu. Whether
  Pocketwatch causes this has not been tested yet.
- The brightness bar counts the PSP-3000's five steps (its backlight levels 44, 60, 72, 84 and
  ARK-4's fifth). On another model it may show the wrong step.
- Not supported: the PSP-1000 for save states (it has too little RAM) and CFWs other than ARK-4.

## Report a problem

Open an issue: <https://github.com/semiloker/pocketwatch/issues/new/choose>. Include your PSP
model, your ARK-4 version and the game's ID, and attach the log that matches where it happened:

| Where | Log |
|---|---|
| In a game | `seplugins/pocketwatch/pocketwatch.log` |
| In the XMB | `seplugins/pocketwatch/xmb.log` and `boot.txt` |
| In Pocketwatch Home | `seplugins/pocketwatch/home.log` |

A log names your game's file (`game: ... from ms0:/ISO/...`). Issues are public, so you may want to
remove that part before you post it.
