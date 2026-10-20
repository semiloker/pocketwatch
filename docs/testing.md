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
| H12 | XMB → Settings → Theme Settings → Theme Color: pick a colour other than the default; start the game, open the hub (Classic style). | The hub's sky has the XMB's colour. Log: `shell: sky N (theme colour M, month 10)`. If the colours differ, write down which colour you picked (its place in the list, counting from 1 after the default) and what the hub showed. |
| H13 | In each style: open the hub, move up and down for about 10 s, close it; once in Castlevania and once in another game. | Moves look smooth. Log after each: `hub: style S, T ms: N frames drawn in L vblank waits; a frame: backdrop .. us, content .. us, present .. us, worst .. us; waiting .. us a loop` and `cpu: hub, T ms: <thread> NN% ...` - send these lines. |
| H15 | Open the hub and press VOL+ a few times, then VOL- a few times; hold VOL+ at the top; press NOTE (mute) and press it again. Do it in each style. | A volume bar appears at once and fades out about 1.5 s after the last press: Classic at the bottom (speaker and 30 steps), Modern a pill on the right, Minimal a line with the number. The level glides to the new value; at the top a press still shows it; muted, the speaker is crossed out. Compare Classic with the PSP's own bar in a game (no hub): write down what differs, or a photo of both. |
| H16 | Open and close the hub five times in a game. | The hub appears as soon as HOME goes down. Log, each open: `shell: open: freeze .. ms, GE .. ms, screen .. ms, first frame .. ms after the press` - send these lines (the target is 300 ms or less). |
| H17 | Need for Speed: Most Wanted (or another game that loads while it plays): press HOME during a race, in a menu and on a loading screen, ten times in all; close the hub each time. | The hub opens every time; the game never stays frozen. Log, each open: `shell: open: freeze .. ms (the card held N time(s)), ...` - send these lines; N above 0 is the case that froze before. If a press is ignored: `shell: HOME ignored: the game kept the Memory Stick busy` (press again). |
| H18 | In a game: HOME → Screenshot; Back to the game; take two more at other places. Then Home → Photos. | "Saved to Photos: PICTURE/<disc ID>" after a moment; the game goes on as it was. Home shows an album under the game's name with the three pictures (the game alone, no menu over it); the XMB's Photo shows the folder. |
| H14 | (debug build) Anywhere in the menu press SELECT. | `seplugins/pocketwatch/shot-000.png` (then 001, ...) is the exact picture on the screen. Send shots of anything that looks wrong, e.g. icons that look banded. |

### Settings (in the XMB since ADR-024)

| ID | Do | Expect |
|---|---|---|
| ST1 | XMB → Settings → Pocketwatch → Fast Save and Load → Off. Start the game, save to a slot twice. | Both saves sleep for real (dark ~5 s each); the log says `engine: fast mode (touch-and-go suspend, no sleep) OFF`. Set it back to On. |
| ST2 | Pocketwatch → Clock and Battery → Off; start the game, open the hub. | No clock or battery in the corner; On brings them back (next game start). |
| ST3 | Pocketwatch → Language → Deutsch; start the game. Then Language → System Language. | The hub in German; `seplugins/pocketwatch/language.txt` contains `de`; System Language removes the file. The XMB's Pocketwatch rows follow the choice at once. |
| ST4 | In Pocketwatch, Back / Circle in a value list. | The list closes without a change. |
| ST5 | Pocketwatch → Style → Modern, start the game: the hub, Save State list, Load State list, a dialog (Quit Game → No). Then Minimal, then Classic (XMB). | Modern: the game blurred behind, round buttons at the bottom with a white disc moving between them, cards and pill buttons. Minimal: the game darkened, only text in a centred column. Note anything cut off, overlapping or slow. |

### ARK's exit combos

| ID | Do | Expect |
|---|---|---|
| SH2 | Owner report 2026-10-17: SH1's first step turned the PSP off. In PLUGINS.TXT set the Pocketwatch line to `off`, start the same game, press L+R+START+DOWN. Then XMB → Game → Memory Stick (or ARK's "Custom Launcher" item): start ARK's menu directly. Set the line back to `on`. | Write down for each: ARK's menu appeared / the PSP turned off (power light off) / black screen with the light on. If it also turns off without Pocketwatch, it is ARK's launcher or another plugin, not ours. |
| SH1 | In gameplay press L+R+START+DOWN (ARK's exit). Start the game again, open the hub, press it there. Then Save State to a slot while holding L+R+START+DOWN. | In gameplay it exits as usual. In the hub nothing happens (use Quit Game). The save finishes normally and the PSP stays in the game. |

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

### M13

The 3rd Birthday rebooted into the XMB during the first save's sleep (2026-10-17, RISK-016). To
find out whether the game or our save is the cause:

1. Start The 3rd Birthday, play to a calm spot. **Without opening the hub**, slide POWER down
   (sleep), wait 10 s, slide it up. Does the game continue? (no: the game itself does not survive a
   sleep here - nothing Pocketwatch can fix; stop.)
2. If it continued: now hub → Save State → a slot. Does it come back? (the first save sleeps for
   real)
3. If 2 failed: restart, sleep once with the POWER switch as in step 1 (that gives the session its
   real wake), then hub → Save State: this save is a fast one, without a real sleep. Does it work?
4. Send the log and which steps worked.

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
| T3 | (Failed on 2026-10-17 with "another setup" because of a bug, fixed: run it again.) Save to slot 1. Quit Game. Start the same game again, get to gameplay (or the title screen). Hub → Load State → slot 1 → Yes. | "Loading... %" then the saved moment, sound and controls working. Log: `engine: adopted a state from the Memory Stick`, `engine: ---- LOAD`, `store: after the load: the card is current`. |
| T3b | Right after T3: play 1 minute, make a save **in the game**, then Quit Game. | The XMB comes back normally (no black screen: this checks the IPL seeds after a cross-boot load). Start the game again: its own save loads. |
| T6 | Save to slot 2. Turn the PSP fully off (hold POWER up 3 s). Later (an hour, a day) turn it on, start the game, load slot 2. | As T3. Note how long the PSP was off. |
| T5a | Rename the game's ISO on the PC (keep the extension), start it from the new name, load slot 1. | "This state cannot be loaded." / "This state belongs to another game or ISO."; nothing else happens. Rename it back. |
| T5b | Deploy another build (`scripts\build.cmd -Release`, then `scripts\deploy.cmd`), start the game, load a slot saved with the debug build. | "...was saved with another setup."; log `engine: adopt: saved in another setup: build ... prx .../...`. Deploy the debug build again. |
| T5c | Start a different game. | Its lists show none of the first game's slots. |

### Battery and Play Time (branch feature/psp-xmb-overhaul)

| ID | Do | Expect |
|---|---|---|
| B1 | Restart the PSP. At once XMB → Settings → Battery; back out. Wait 3 minutes in the XMB, open Battery again; plug the charger in, wait a minute, open it again. | Charge and Status as the XMB's own battery icon; Time left "... (system)" at first, "about ..." after two minutes or more; with the charger "Charging" and, after a minute, "Time to full". Voltage 3.6-4.2 V, Current in mA, Temperature 15-45 °C, Capacity "N of M mAh" (N grows while charging), Charge cycles a number - or "Not available". Send `xmb.log` (`xmb: battery ...`). |
| P1 | Note the time. Play 20 minutes with: a hub visit of 2 minutes, a save, a load, a sleep of 1 minute (POWER switch). Quit Game. XMB → Game → Play Time. | Total and the game's row grew by about 20 minutes (not by the hub, the sleep or the load); other games below it, most played first. Game log lines `playtime: N s this session written (...)`. Files `seplugins/pocketwatch/playtime-a.txt` / `-b.txt`. |

### Pocketwatch in the XMB's Settings (VSH-REQ-006)

Install with `scripts\deploy.cmd -EnableXmb` (adds `VSH, ms0:/seplugins/pocketwatch/pocketwatch_xmb.prx, on`
to PLUGINS.TXT). If the XMB does not come up: turn the PSP off, hold START while turning it on (ARK-4
then loads no plugins), connect USB and remove that line.

| ID | Do | Expect |
|---|---|---|
| X1 | Restart the PSP. XMB → Settings: "Pocketwatch" and "Battery" right after System Settings; XMB → Game: "Play Time" right after Saved Data Utility. Open Pocketwatch; change Style to Modern, Fast Save and Load to Off, Language to another one; back out. Open System Settings, then ARK's Custom Firmware Settings. | The three items with the icon of the one before them; Pocketwatch lists Style / Fast Save and Load / Clock and Battery / Language with the XMB's own value popup. System Settings and ARK's pages look as before. In a game the hub has the chosen style; `seplugins/pocketwatch/config.txt` shows the values. Send `seplugins/pocketwatch/xmb.log` (`xmb: vsh_module patched ...`, `xmb: Pocketwatch and Battery added ...`, `xmb: Play Time added ...`, `xmb: sysconf_plugin patched`). Partly seen 2026-10-17 (the first item and both patches). |
| X1b | If X1 shows no item: send `xmb.log` (it says which site was missing). | - |

### Pocketwatch Home (HOM, ADR-025)

Install: `scripts\deploy.cmd` copies `home\EBOOT.PBP` to `PSP/GAME/PocketwatchHome/`. Start it
from XMB → Game → Memory Stick → Pocketwatch Home.

| ID | Do | Expect |
|---|---|---|
| HM1 | Start Home. Move left and right through the games for a while; rest on one for a second; L / R through the sections. | Every ISO and CSO of `ms0:/ISO` with its own icon (icons appear one by one at first); resting on a game fades its own background art in; under the title the disc ID, play time and save states where there are some. Movement smooth. Note anything missing, wrong or slow (how long until the first screen). |
| HM2 | On a game press X (Enter). | The screen fades out and the game starts; the plugin's hub works in it as usual (HOME). Quit Game returns to the XMB. |
| HM3 | Play Time section; System section: wait a minute; Exit to the XMB. HOME in Home. | Play Time lists the games as the XMB's Play Time does, with bars; System shows the battery like the XMB, free space, the number of games; Exit and HOME (Quit) go back to the XMB. |
| HM4 | Save a state in a game started from the XMB, Quit Game, start the same game from Home, Load State; then the other way round. | Either it loads, or "This state was saved with another setup" (then the two starts differ: RISK-020). Send the log. |
| HM5 | XMB → Settings → Pocketwatch → Start screen → Pocketwatch Home. Do not open Home from the Game menu in between. Switch the PSP off (hold POWER up) and on. Note the seconds from power-on to Home's first screen. | Home appears instead of the XMB. `seplugins/pocketwatch/boot.txt`, last line: `... start: Pocketwatch Home (...)`. If the XMB appears: send `boot.txt` and `xmb.log` (a line `... but the XMB started` and the launcher field's value). |
| HM6 | In Home: System → Exit to the XMB. Then start any game from the XMB and quit it. | The XMB appears (once); after the game, Home again. Same for HOME → Quit in Home. |
| HM7 | In Home start an ISO, a PS1 game (Final Fantasy Tactics, Metal Gear Solid) and a homebrew (PSP Filer); quit each. | Each starts; PS1 icons are square, not stretched; quitting returns to Home. |
| HM8 | Switch on holding L. Then switch on holding START. | Holding L: the XMB with Pocketwatch's pages, `boot.txt` says `L held`. Holding START: the XMB without plugins (ARK-4); the next normal start is Home again. |
| HM9 | Back to normal: Settings → Pocketwatch → Start screen → XMB, switch off and on. | The XMB, `boot.txt`: `start = xmb`. |
| HM10 | In Home: move left and right through the games quickly for a minute, switch sections, wait 30 s. | Smooth movement. `seplugins/pocketwatch/home.log`: `home running: ... (NN fps), N late; a frame: CPU ... us, warm ... us, GE wait ... us; longest ... ms; stamps ...` - expect about 60 fps, few late frames, no frame over 100 ms, and `rows` (the slowest refresh of the sections' rows) under 2000 us; once `home reads: ...` (what each battery call cost). Then change a few settings and play with Music for 30 s: the next line the same. Send the file. |
| HM11 | In Home press HOME; move through the panel; HOME again; HOME → Sleep (wake it), HOME → Exit to the XMB. | HOME opens Pocketwatch's panel (never the system's quit dialog), HOME or Back closes it; Sleep sleeps and Home is there after waking; Exit gives the XMB. |
| HM13 | Home → Settings: change Start screen, Style, Time Format (watch the clock), Backlight Brightness (watch the screen), AVLS; Nickname → type a name → Start. Then System. | Each value changes at once and stays after leaving Home and coming back; a value the system refuses shows "Not available". System shows the model, 6.61, ARK-4's version, the MAC address and the new nickname. |
| HM14 | Home → a game with saves of its own → Triangle → Saved Data. | The firmware's saved-data list of that game opens over Home; Back returns to Home. |
| HM15 | Save a state in a game (hub), quit, Home → that game → Triangle. Choose the state. Then Triangle on any game (a homebrew too) → Information, Back. | The state is listed with its picture and time; choosing it starts the game and the state loads once it runs. Information shows the disc ID, version, system software, size and file; Back returns to the options. Send pocketwatch.log. |
| HM16 | Home → Music: play a track, switch sections, pause, skip with left / right, let a track end; then start USB. | Music plays on while Home is browsed; the time and bar move; at a track's end the next one starts; USB stops the music. Note any track that does not play (send its name). |
| HM18 | Home with the volume up: move through games, rows and sections, open and close the HOME panel and a game's options, change a setting, try Wallpaper → right without a wallpaper; play music and move. Settings → Key Sounds → Off. | Each press has a short soft sound (move, section, open, back; a low one for the refused setting) that never lags the screen; with music they mix over it; Off silences them, also after a restart. Say if any is too loud, too sharp or missing. |
| HM19 | WLAN switch on, a connection saved in the XMB. Home → System → Internet Browser; choose the connection; open a page; leave the browser (its own exit). Then the Music, Settings → Nickname and a game's Saved Data again. | The system's browser opens over Home, connects, shows FrogFind (or `browser.txt`'s page); leaving it gives Home back where it was. If it does not open: the row says "Not available". Send `home.log` (the `home browser` line: each step's result, `free_kb`, `pool_mb`, `used_kb`; the `heap ... outside` figures). |
| HM17 | Home → Photos: move through the pictures, Enter on one, left / right, Triangle (wait 15 s), Square, Back. Go to Settings; restart Home; Settings → Wallpaper → left. | Thumbnails appear; the chosen picture shows blurred behind the row; Enter shows it whole with its name and the buttons for 2 s, left / right change it; Triangle goes on by itself every 5 s; Square makes it the background of Settings, System, Music and Play Time (after a restart too); Wallpaper → Pocketwatch brings the old background back. |
| HM20 | Home → Settings → Style: choose Classic (XMB), then Modern, then Minimal; in each go through every section. In Classic: left / right, up / down; on Settings → Time Format press Enter, choose with up / down, Enter; Back. | Home's look changes at once each time. Classic looks like the XMB: the XMB's own category icons (game pad, note, camera, toolbox...) and its sky with the moving wave (in the theme colour you chose in the XMB); left / right switch categories; Enter on a setting opens its options on the right. Send `home.log` (the `home art: N system picture(s), sky S (theme colour C)` line) and a photo of anything that looks wrong. |
| HM21 | Modern: Photos (up / down a row, left / right one), Music with a track playing, Play Time, Settings (the switches), System. Minimal: the same sections. | Modern: pills at the top, the chosen row a white pill that glides, switches for On/Off, the games' icons in Play Time, the playing track's card on the left of Music, a grid of photos. Minimal: plain words, the chosen line in the middle. Say what still looks wrong, with a photo. |
| HM22 | Home → Settings: open each category (Enter), change a value, Back; in Appearance try every theme and accent and each background; Battery Percentage off and on. System: open Battery, Memory Stick, About This PSP, Power. Do it in each style. | Each page slides in from the right and back out (Classic: the XMB's option list, the categories sliding left, the system's own Settings icons); the look changes at once and stays after a restart (`home.txt`). Send `home.log` (the `home art:` line: how many items). |
| HM23 | On a game with saved data: Triangle → Saved Data. Move through it; open one; then Delete on a save you do not need (or a copy of one), Cancel first, then Delete. Triangle → Save States: Square on a state, Cancel. | The saves show their pictures, titles and times, the chosen one's picture behind; Delete asks first and removes only that save (check on the PC); Cancel keeps it. |
| HM24 | Music with tagged MP3s: play one; Triangle (Now Playing); right once, left once; hold right 3 s; down, up; Square (shuffle), Triangle (repeat) a few times; let a track end. | Titles, artists and albums from the tags; the cover on the card and behind; the hints say ◀▶ Seek and ▲▼ Track; a press of right / left jumps 10 s, holding keeps seeking (5 s steps), the sound follows; down / up change the track; repeat One plays the track again; shuffle never repeats one at once. Send `home.log`: its `home cover:` lines (the picture's size and the ms it took) and `home music: a track started in N ms` (ADR-039: how slow a cover and a track change are). Name any track whose cover or tag is wrong. |
| HM25 | Photos: open All Photos and a game's album (one with HOME → Screenshot pictures); in Modern press right along a row and down a row; a photo whole; Enter (the editor): left / right through the tools, up / down on each, Save a Copy; Square on a photo in an album (Cancel). Games: Triangle on that game. | Albums by folder, a game's screenshots under its name, each named by its date and time ("2026/10/20 14:05") with the game's title under it (in All Photos too); Modern's grid moves a whole row only when the choice goes to another row; the game's page has Photos (N) opening its album; the preview changes with every press; Save a Copy writes `<name>_edit.jpg` beside it (shown first in the album). |
| HM26 | Play music in Home, slide HOLD on, wait 2 s, then off. Settings → Sound and Display → Screen Off with HOLD → Always: HOLD without music. Music and Photos → Sleep Timer → 15 min (wait, or note the time). Appearance → Clock → each; Background → Aurora. Games: Square on two games. | HOLD: the screen goes dark after half a second and the music plays on; HOLD off: the screen back at the same brightness. After 15 min the music stops and the PSP sleeps. The clock as chosen ("Mon 14:05"). The aurora's lights drift slowly behind everything. The starred games come first with a star. |
| HM27 | Home → Settings → the nickname: Enter. Type with the d-pad and Enter; Square, Triangle, L / R, SELECT (each page), Shift then a letter; Back (Keep Editing), Back again → Discard; open it again, change it, START. | Home's own keyboard, in the style's colours (no firmware keyboard): the letters, Cyrillic and signs pages; Shift makes one capital; Square erases before the caret, Triangle types a space, L / R move the caret. Back with changes asks Save / Discard / Keep Editing in the same look; Discard keeps the old name; START saves it (the row and the XMB show the new nickname). |
| HM28 | Start a game from Home, play 10 s, HOME → Quit Game. Home → Play Time; the game's row in Games. | The game is listed with its seconds (e.g. "10 s"), and minutes once past a minute. |
| HM12 | With the cable in the PC: HOME → USB Connection (once). Wait up to 10 s. If the PC shows nothing, press Enter (Connect Again) once. Copy a file, then End (Back). Then start USB without the cable, plug it in after 5 s. | The PC sees the Memory Stick after one press (the card says Connecting..., then Connected); End gives Home back with the games read again. Without the cable: "Connect the USB cable", and plugging it in connects by itself. Send `home.log`: two `home usb:` lines per connection (the drivers; then the states the link went through) - and say whether Connect Again was needed. Keep RemoteJoyLite on as it is (ADR-034: Home now takes the port from it; the line says `bus ... (another's)`, `other device off 0`). |

### Continue from a state at the game's start (M7, GAME side)

The XMB entry does not exist yet; this tests the half that loads the state.

| ID | Do | Expect |
|---|---|---|
| V1 | After T3 passes: save to slot 2, Quit Game. On the PC create `seplugins/pocketwatch/continue.txt` with one line `ULUS10277 2` (your game's disc ID). Start the game and touch nothing. | About 2 s after the game starts: "Loading... %", then the saved moment. `continue.txt` is gone. Log: `states: continue from slot 2 once the game runs`, `shell: states has a start-up action`, then as T3. |
| V1b | Create `continue.txt` with another game's ID (or slot 9) and start the game. | The game starts normally; the file is gone; log `states: continue.txt asks for ...: ignored`. |

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
6. For every other game you play, fill its row in the [compatibility matrix](compat.md) (save to
   a slot, load, load after a restart, ten loads, a save during sound or a menu, Quit Game).

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
