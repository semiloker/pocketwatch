# Device testing guide

Step-by-step tests for every feature on the PSP-3000 (ARK-4 6.61). Each test says what to do, what
you should see, and which log lines prove it. Run them in order: later sections assume the earlier
ones passed. Results go back as the log plus a short note per test ID (section 9).

Specs behind the tests: [game shell](subsystems/game-shell.md), [UI](ui/README.md),
[states engine](subsystems/states-engine.md), [states storage](subsystems/states-storage.md).

## 0. Before you start

1. **Back up the whole Memory Stick** to the PC (copy every folder of the PSP's drive into a folder
   on the PC). Sections 4-6 write to the card and roll the kernel back; a backup makes any failure
   harmless.
2. Battery above 50 % or the charger connected (a save sleeps the PSP for a moment).
3. Only Pocketwatch for states: `seplugins/PLUGINS.TXT` must keep `cushome.prx` and
   `pspstates_cushome.prx` off (test G5 turns one on on purpose). RemoteJoyLite may stay on, but do
   not stream to the PC during the tests; if a save hangs, turn it off first.
4. Build and install (PC, PSP connected by USB): `scripts\build.cmd`, then
   `scripts\deploy.cmd -Enable`. Leave USB mode on the PSP (Circle) before starting a game.
5. Pick one game for sections 1-6 (the examples use Castlevania, ULUS10277). Any ISO or UMD works;
   PSN games (EBOOT.PBP) keep states in RAM only (no slots) and are tested in M12.
6. The log is `<PSP drive>:\seplugins\pocketwatch\pocketwatch.log`. It is appended: before each
   section you may rename the old one to keep runs apart.

**If something goes wrong.** A black screen that does not come back within 20 s: hold the POWER
switch up for 10 s (if it does not turn off, take the battery out). Then start the PSP, check
System Settings (date, time, theme, nickname) and send the log. If the settings are gone, stop
testing: that is the flash1 symptom the engine guards against (E2 run 5). If a game's own save is
damaged, restore it from the backup.

## 1. Install and start

| ID | Do | Expect |
|---|---|---|
| S1 | Start the game. | It boots as without the plugin. |
| S2 | Quit (Quit Game, test Q1) and open the log on the PC. | First lines: `Pocketwatch <version> loaded from ms0:/seplugins/pocketwatch/pocketwatch.prx`, an import self-check with no `MISSING`, `game: ULUS10277 "<title>" from ms0:/ISO/... (... KB)`, `store: 5 slots for ULUS10277, PARAM.SFO hashed`, `assets: ltn0 ...`. |

## 2. HOME hub and the look

| ID | Do | Expect |
|---|---|---|
| H1 | In gameplay, press HOME. | The system HOME screen does **not** appear; Pocketwatch's hub does, over the XMB background of this month with the moving wave; the game is frozen and silent. Log: `shell: HOME opens Pocketwatch`. |
| H2 | Look at the screen next to the real XMB (quit to the XMB later and compare). | Category icon and "Game" label at the top left, rows Resume Game / Save State / Load State / Quit Game with the XMB's own icons, the selected row bright, the others dimmer; clock top right in your date/time format; battery icon; hints "✕ Enter ○ Back" (or swapped if your PSP uses ○ to confirm). Note anything that looks different (font, sizes, colours, positions). |
| H3 | Up/Down, hold Down. | The cursor moves and repeats after a short delay; it stops at the ends. |
| H4 | Press Back, then open again and press HOME. | Both close the hub; the game continues, its buttons work again, no button press "leaks" into the game. |
| H5 | Resume Game. | Same as H4. |
| H6 | Open the hub, leave it 2 minutes. | The screen does not dim or switch off. |
| H7 | Open the game's own save screen (its save menu) and press HOME there. | The hub does **not** open (a system dialog is running). Log: `shell: HOME ignored: a system dialog (saved data, keyboard, network) is open`. |
| H8 | Change the ✕/○ assignment (ARK VSH menu), start the game, open the hub. | The hints and Enter/Back follow the setting. |
| H9 | System Settings → Language: French (then German, Russian), start the game, open the hub, Save State list, a dialog. | Everything in that language, no missing letters (a box or "?" instead of a letter is a fail), long dialog lines wrap instead of ending in "...". Log: `i18n: fr, 58 of 58 strings translated`. Write down any wording that reads wrong. |
| H10 | Put a file `seplugins/pocketwatch/language.txt` containing `uk`, start the game. Delete the file afterwards. | Ukrainian text, or English with the log line `i18n: ... has letters the system font lacks` (then the system font has no Ukrainian letters: report it). |
| H11 | Save to a slot in one language, change the system language, start the game, load it. | It loads; after the load the hub speaks the language of the moment it was saved (by design: the state brings its own). |

### Settings

| ID | Do | Expect |
|---|---|---|
| ST1 | Hub → Settings → Fast Save and Load → Off. Save to a slot twice. Quit, start the game again, open Settings. | Both saves sleep for real (dark ~5 s each); the log says `engine: fast mode (touch-and-go suspend, no sleep) OFF`; after the restart it is still Off (`config.txt`). Set it back to On. |
| ST2 | Settings → Clock and Battery → Off, back to the hub. | No clock or battery in the corner; On brings them back. |
| ST3 | Settings → Language → Deutsch. Back to the hub. Then Settings → Language → System Language. | German at once (hub, lists, dialogs); `seplugins/pocketwatch/language.txt` contains `de`; System Language removes the file and returns to the system's language. |
| ST4 | Back / HOME inside Settings and inside the option list. | Back closes the list without a change; HOME closes too. |

## 3. Quit Game

| ID | Do | Expect |
|---|---|---|
| Q1 | Hub → Quit Game → Yes. | The XMB comes back within ~5 s. Log: `shell: Quit Game`, `engine: exit: IPL seeds present`, `shell: quit: the game's exit callback was invoked`. |
| Q2 | Hub → Quit Game → No / Back. | Back in the hub; the game continues after Resume. |
| Q3 | After Q1: start the same game again. | It boots normally; System Settings unchanged. |

## 4. States on the Memory Stick

The first save of a session puts the PSP to sleep for real (the screen goes dark ~5 s); later saves
and loads use the fast suspend (~2-3 s dark). Then the state is written to the card ("Saving... %").

| ID | Do | Expect |
|---|---|---|
| M1 | Hub → Save State. | A list in the Saved Data Utility layout: heading "Save State", the game's title on the right, five rows "New Save State (1)".."(5)" with a plus. |
| M2 | Choose New Save State (1). | "Saving..." with the busy ring, a dark moment, then "Saving... 0..100%", then the hub again: Save State says "1 of 5 slots used", Load State says "Save State 1, saved at HH:MM". Log: `store: state written to slot 1 (... KB)`. Note the seconds from Enter to the hub. |
| M3 | Resume, play 1 minute (change something visible: position, health, money). Hub → Load State. | The list shows only Save State 1 with a thumbnail of the moment, date/time in your format and the size in MB. |
| M4 | Choose it → Yes. | "Loading..." (no percent: the state is still in RAM), a dark moment, the game at the saved moment, the hub closed. Log: `store: slot 1 is the state held in RAM: no read needed`, `engine: ---- LOAD: back in the saved moment ----`, `store: after the load: the card is current (marker ...)`. |
| M5 | Play 30 s, load slot 1 again. | Now "Loading... 0..100%" (read from the card), then the saved moment. Log: `store: slot 1 read (... KB), ready to load`, `engine: adopted a state from the Memory Stick`, `store: after the load: the card is current`. |
| M6 | Save to slot 2 and slot 3 at different moments; load 3, then 2. | Each load returns to its own moment. |
| M7 | Save State → choose the used slot 2. | "Overwrite Save State 2?" with No selected first; Yes overwrites (new thumbnail and time), No goes back to the list. |
| M8 | In either list, triangle on slot 3 → Yes. | The slot becomes "New Save State (3)" (Save list) or disappears (Load list); the folder `PSP/SAVEDATA/PWULUS1027703` is gone on the PC. |
| M9 | Quit the game, XMB → Game → Saved Data Utility. | Entries titled with the game's title, "Save State 1"/"Save State 2", detail "Pocketwatch", the thumbnail as icon and the screenshot as background. Delete one there; start the game: our list shows that slot empty. |
| M10 | On the PC, open `PSP/SAVEDATA/PWULUS1027701/DATA.BIN` in a hex editor and change one byte in the middle (or truncate the file). Load slot 1. | "This state cannot be loaded." with "This state is damaged." (or "...could not be written or read."); the game keeps running untouched. Restore the file from the backup afterwards. |
| M11 | Fill the card (copy a big file until less than ~20 MB is free), save to a new slot. | "The state could not be written to the Memory Stick." with the reason; the game continues; the slot stays empty. Free the space again. |
| M12 | A PSN game or homebrew, if you have one: hub → Save State, Load State. | No list: the state stays in RAM (as before E3), Load State loads it once. Log: `store: no game ID: states stay in RAM`. |

### T1b

The Memory Stick check of T1 passed with a state saved through a real sleep. This repeats it for a
state saved with the fast suspend (debug build only):

1. Start the game, hub → Save State → any slot (this first save sleeps for real).
2. Hub → Memory Stick Test → Yes. Its save is now a fast one.
3. Expect the message that the test passed. Log: the test's save line is
   `engine: save: requesting a touch-and-go suspend`, then `cardtest: probe A found`,
   `probe A contents match`, `probe B written 1, read back ok; probe A afterwards ok`.
4. Quit, run the PC FAT check (section 6, step 4). Delete the `t1a-*.bin` / `t1b-*.bin` files in
   `seplugins/pocketwatch/` afterwards.

If T1b fails, stop using states on this card until it is fixed and send the log and the check's
output.

## 5. Load after a reboot (E3)

| ID | Do | Expect |
|---|---|---|
| T3 | Save to slot 1. Quit Game. Start the same game again, get to gameplay (or the title screen). Hub → Load State → slot 1 → Yes. | "Loading... %" then the saved moment, sound and controls working. Log: `engine: adopted a state from the Memory Stick`, `engine: ---- LOAD`, `store: after the load: the card is current`. |
| T3b | Right after T3: play 1 minute, make a save **in the game**, then Quit Game. | The XMB comes back normally (no black screen: this checks the IPL seeds after a cross-boot load). Start the game again: its own save loads. |
| T6 | Save to slot 2. Turn the PSP fully off (hold POWER up 3 s). Later (an hour, a day) turn it on, start the game, load slot 2. | As T3. Note how long the PSP was off. |
| T5a | Rename the game's ISO on the PC (keep the extension), start it from the new name, load slot 1. | "This state cannot be loaded." / "This state belongs to another game or ISO."; nothing else happens. Rename it back. |
| T5b | Deploy another build (`scripts\build.cmd -Release`, then `scripts\deploy.cmd`), start the game, load a slot saved with the debug build. | "...was saved with another setup."; log `engine: adopt: saved in another setup: build ... prx .../...`. Deploy the debug build again. |
| T5c | Start a different game. | Its lists show none of the first game's slots. |

## 6. File-system integrity (E4)

Run after section 5, with the backup at hand.

1. On the PC, record the card's files (PowerShell, `D:` = the PSP drive):
   `Get-ChildItem D:\ -Recurse -File | Where-Object { $_.FullName -notmatch 'SAVEDATA\\PW|seplugins\\pocketwatch' } | Get-FileHash | Select-Object Hash, Path | Export-Csv before.csv`
2. On the PSP, 10 cycles: save to a slot → play 1 minute → save **in the game** → load the slot →
   play 30 s. Use slots 1-5 in turn. Quit Game at the end.
3. Record again as `after.csv`, then compare:
   `Compare-Object (Import-Csv before.csv) (Import-Csv after.csv) -Property Hash, Path`.
   Expected differences: only the game's own save folder (the in-game saves).
4. FAT check: a Command Prompt **as administrator**, `chkdsk D:` (without `/f`). Expected:
   "Windows has scanned the file system and found no problems." If it reports errors, do **not**
   run `/f`; send the output and the log, then restore the backup.
5. The full gate (T7) is 100 cycles on five games with the check every 10 cycles; run it when the
   10-cycle run is clean.

## 7. Guards

When a guard applies, Save State and Load State are grey with the reason under them; Enter on them
explains what to do. The hub checks every second, so the rows come back as soon as the cause is
gone. Log: `guard: states refused: <reason>` and `guard: states allowed again`.

| ID | Do | Expect |
|---|---|---|
| G1 | Without the AC adaptor, with the battery below 15 % (play until the XMB shows it low), open the hub. Then plug the adaptor in. | "The battery is low." / Enter: "Connect the AC adaptor."; with the adaptor the rows are available again within a second. |
| G2 | Only if a plugin or the game can turn USB mass storage on in a game: turn it on, open the hub. | "Not available while USB mode is on." |
| G3 | A game with ad hoc or online play: enter its network mode (WLAN switch on), open the hub; leave network mode, open again. | First "Not available while the wireless network is in use."; afterwards available. |
| G4 | ARK settings: high memory (extra RAM) on for the test game; start it, open the hub. Turn it off again afterwards. | "Not available for this game (extra memory mode)." |
| G5 | Turn `cushome.prx` on in PLUGINS.TXT (keep Pocketwatch on), start the game, open the hub (HOME or NOTE). Turn it off again. | "Another save state plugin is running." / "Turn off Custom Home and PSPStates in PLUGINS.TXT." Note which menu HOME opened. |
| G6 | Open the hub, slide POWER down (sleep), wait 10 s, slide it up. | The hub is shown again correctly (background, icons, text, not garbage); Resume → the game runs with sound. |
| G7 | Right after a load (M5), press HOME at once, then again after 3 s. | The first press does nothing (log `shell: HOME ignored: the PSP has just resumed`), the second opens the hub. |

## 8. Release build

1. `scripts\build.cmd -Release` and `scripts\build.cmd -Release -Target check` (must end with
   "All checks passed"), then `scripts\deploy.cmd`.
2. Repeat S1, H1, H4, Q1, M2, M5, T3. The hub has no Memory Stick Test; the debug button combos do
   nothing.
3. Deploy the debug build again for further testing (`scripts\build.cmd`, `scripts\deploy.cmd`).

## 9. What to send back

- `seplugins/pocketwatch/pocketwatch.log` (the whole file).
- Per test ID: pass / fail, and for a fail what you saw (a phone photo of the screen helps).
- Times you noticed (M2 save, M5 load, T3 load), and anything in the look (H2) that differs from the
  real XMB.
- The `chkdsk` output and the `Compare-Object` result of section 6.
