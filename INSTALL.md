# Installing Pocketwatch on a PSP

This takes about five minutes. It changes nothing in the PSP's firmware: every file goes on the
Memory Stick, so taking the files away undoes it.

> **Pocketwatch is a pre-release, tested on one PSP-3000 with ARK-4 on firmware 6.61.** Save states
> are experimental. Back up your Memory Stick first, at least the `PSP/SAVEDATA` folder.

## What you need

| | |
|---|---|
| PSP | A PSP-3000 is the tested model. The PSP-2000, Go and Street are untested. The PSP-1000 cannot use save states (it has too little RAM), and the rest is untested on it. |
| Custom firmware | [ARK-4](https://github.com/PSP-Archive/ARK-4) on firmware 6.61, already installed and running. Other custom firmwares are not supported. |
| To copy the files | A USB cable and a computer, or any way to reach the Memory Stick |

Check first that nothing else takes over saving or the HOME button: in `seplugins/PLUGINS.TXT`,
turn **off** PSPStates and Custom Home. Running two savestate plugins at once is not supported.

## Install

### 1. Download

Get `pocketwatch-<version>.zip` from the [Releases page](https://github.com/semiloker/pocketwatch/releases)
(under **Assets** of the newest release) and unzip it on the computer.

### 2. Copy the files

Connect the PSP: XMB → Settings → USB Connection. Inside the unzipped `pocketwatch-<version>`
folder are two folders, `seplugins` and `PSP`. Copy **those two** to the **root** of the Memory
Stick, the top level where a `PSP` folder already exists, and merge them with the folders already
there. Do not copy the `pocketwatch-<version>` folder itself.

The Memory Stick should then have these, next to your own files:

```text
PSP/GAME/PocketwatchHome/EBOOT.PBP       Pocketwatch Home
PSP/SAVEDATA/ARK_01234/PWHOME.PBP        Pocketwatch Home as a start screen (ARK-4 runs it from here)
seplugins/pocketwatch/pocketwatch.prx    the game plugin: the menu and the save states
seplugins/pocketwatch/pocketwatch_xmb.prx    the XMB plugin: its settings pages
seplugins/pocketwatch/pocketwatch_perf.prx   the overlay (off until you turn it on)
seplugins/pocketwatch/lang/              translations
```

### 3. Turn the plugins on

Open `seplugins/PLUGINS.TXT` on the Memory Stick in a plain text editor such as Notepad. If there
is no such file, create it. Add these two lines exactly:

```text
UMD, ms0:/seplugins/pocketwatch/pocketwatch.prx, on
VSH, ms0:/seplugins/pocketwatch/pocketwatch_xmb.prx, on
```

- `UMD` is not a mistake: it loads the plugin for retail games only, both discs and ISOs. `GAME`
  would load it in homebrew too.
- Do **not** add a line for `pocketwatch_perf.prx`. Pocketwatch Home writes that one when you turn
  the overlay on.
- PSP Go, on its internal storage: write `ef0:` instead of `ms0:` (untested).

### 4. Restart

Disconnect USB, then turn the PSP off fully and on again (hold the power switch up for a few
seconds; putting it to sleep is not enough). With a **Live** install of ARK-4, run ARK Loader again
after that, as after every power-off: without it no plugin loads.

### 5. Check that it worked

| Where | What you should see |
|---|---|
| XMB → Settings | **Pocketwatch** and **Battery** pages |
| XMB → Game | **Play Time** |
| XMB → Game → Memory Stick | **Pocketwatch Home** |
| In a game, press HOME | Pocketwatch's menu: Resume Game, Save State, Load State, Screenshot, Quit Game |
| On the Memory Stick | `seplugins/pocketwatch/pocketwatch.log`, after starting a game. Its first lines give the version. |

If something is missing, see [If something goes wrong](#if-something-goes-wrong).

## Optional: start in Pocketwatch Home

To make Home what you see when the PSP starts: XMB → Settings → Pocketwatch → Start screen →
Pocketwatch Home, then restart. To get back to the XMB from Home, press HOME and choose **Exit to
the XMB**, or set the Start screen back to XMB.

The overlay and the volume and brightness bars are set in Home: Settings → Pocketwatch → Overlay
in Games, and Settings → Sound and Display → Volume and Brightness. The overlay in games is
experimental; leave it off unless you are testing it.

## If something goes wrong

**The PSP does not reach the XMB, or hangs at start.** Hold **START** while switching on: ARK-4
then loads no plugins. Connect USB and change the two lines' `on` to `off` in `PLUGINS.TXT`. If Home
is the start screen and never appears, the next start goes to the XMB by itself.

**You want the XMB once, with Home as the start screen.** Hold **L** while switching on.
On a permanent ARK-4 install with the new cIPL, ARK-4 uses L at power-on to start DC-ARK, if you
have it; in that case use Home's **Exit to the XMB** instead.

| What you see | What to check |
|---|---|
| No Pocketwatch pages in the XMB | The `VSH` line: spelled exactly, ending in `on`, in `seplugins/PLUGINS.TXT`. ARK-4 running? (Live install: run ARK Loader.) |
| HOME in a game opens the system's menu | The `UMD` line. It covers retail games only, not homebrew. Is another plugin (Custom Home) using HOME? If the system's HOME screen was already turned off, tap the NOTE button instead. |
| No log file after a game | The game plugin did not load: same checks as above, and the file must be at `seplugins/pocketwatch/pocketwatch.prx` |
| "Pocketwatch Home" is missing from Memory Stick | The `PSP` folder went to the wrong place: it must merge with the Memory Stick's own `PSP` folder |
| A state refuses to load | By design a state loads only on the setup that made it: the same Pocketwatch version, game file and PSP. After an update, save anew. |

Known problems are listed in each [release's notes](https://github.com/semiloker/pocketwatch/releases).
To report a new one, [open an issue](https://github.com/semiloker/pocketwatch/issues/new/choose).

## Updating

Copy the new zip's `seplugins` and `PSP` folders over the old ones, as in step 2, and restart.
`PLUGINS.TXT` needs no change. Save states from the older version will be refused, by design, so
keep playing from your normal game saves or stay on the old version until you reach a save point.

## Uninstalling

1. If Home is the start screen, set XMB → Settings → Pocketwatch → Start screen back to XMB.
2. Delete the Pocketwatch lines from `seplugins/PLUGINS.TXT`.
3. Delete the `seplugins/pocketwatch` and `PSP/GAME/PocketwatchHome` folders and the file
   `PSP/SAVEDATA/ARK_01234/PWHOME.PBP`. Keep the rest of `ARK_01234`: it is ARK-4's own folder.
4. Save states are in `PSP/SAVEDATA/PW<game ID><slot>`; delete those too if you do not want them.
