# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Changed
- Pocketwatch Home's photo editor, reworked in a professional layout (#8): the picture in its own
  canvas with a histogram, a top bar with Cancel and Save a Copy, three tabs (Adjust, Filters,
  Crop) between L and R, a ruler slider with the value, hold Square to compare with the original,
  Triangle resets a tool. New tools: exposure, highlights, shadows, vibrance, tint, sharpening, the
  aspect ratio; filters with thumbnails and a strength; sliders from -100 to 100.
- Pocketwatch Home's player: every key does one thing, always. Left / right seek (held: on in
  steps), up / down the previous / next track, Enter play / pause, Square shuffle, Triangle
  repeat. Each button shows its key under it, and the one you press lights up with what it did.
  Before, up and right (and down and left) could do the same, depending on what was chosen.

### Added
- The brightness is a vertical bar at the left - its step on top, the steps lit from the bottom,
  a sun - in Home and in games; the volume keeps its pill at the bottom (#7).
- Volume and Brightness → Everywhere: the brightness bar in the XMB too, drawn by the overlay
  module, which does nothing else there (#7, ADR-059; test it after PF-FREEZE).

### Fixed
- Pocketwatch Home's player: holding left / right no longer jumps 10 s every tenth of a second.
- Pocketwatch Home's player: left / right seek on from the new time; the track no longer starts
  again from 0:00 (with Repeat: All it skipped to the next one). Seeking uses the MP3 library's
  own call (#5). An MPEG-2 MP3 (16-24 kHz) shows its real length, not twice it.
- The volume and brightness indicator counts the PSP's five brightness steps, not four ("4/4"
  at the fourth): in Home and in games (#6).
- `scripts\deploy.cmd` finds PLUGINS.TXT lines that end in CRLF (the file after an editor saved
  it): it reported them as missing, and `-Enable` / `-ProbeOff` would have added a second line
  instead of changing the first. It now also appends in the file's own line endings.

## [0.1.0-alpha.1] - 2026-10-29

The first pre-release, tested on one PSP-3000 with ARK-4 on 6.61. The release's notes
(`.github/release-notes.md`) list its known issues.

### Added
- The release zip holds every part: the game plugin, the XMB plugin, the overlay module, the
  translations and Pocketwatch Home, including its start-screen copy in `PSP/SAVEDATA/ARK_01234`.
- Pocketwatch Home: the player is driven by what it shows - up / down between the time bar and
  its five buttons, left / right seek or choose a button (in a ring, its name under it), Enter
  presses it; START pauses on the music pages.
- In a game, HOME -> Screenshot saves the game's picture (without the menu) to
  PICTURE/<game ID>/: Pocketwatch Home shows it in the game's album, the XMB in Photo.
- Pocketwatch Home: the console's Enter button (Cross or Circle) and Ad Hoc channel in Settings.
- Pocketwatch Home: HOLD turns the screen off while music plays (and the CPU slows down to save
  the battery), a sleep timer, the weekday in the clock, favourite games (Square: first, with a
  star) and the Aurora background - softly drifting lights in your accent's colours.
- Pocketwatch Home: every section opens pages of its own that slide in and out. Settings is
  now seven categories (Appearance, Pocketwatch, Music and Photos, Sound and Display, Date and
  Time, System, Network), System has Battery, Memory Stick, About This PSP and Power pages; in
  Classic they are the XMB's own Settings items with the system's icons.
- Pocketwatch Home: themes (Dark, Glass, OLED Black, Paper), nine accent colours, a choice of
  background (the XMB's sky in any month's colour, a gradient, plain, your wallpaper), the wave
  on or off, the battery's percent, quicker or gentler animations.
- Pocketwatch Home: a game's saved data in Home's own look - pictures, titles, details, size,
  Delete (with a question); save states on their own page (continue, delete).
- Pocketwatch Home: music shows the tags' titles, artists and albums and the album cover (also
  blurred behind it); Now Playing; previous / next, hold to seek, repeat and shuffle.
- Pocketwatch Home: photo albums by folder (a game's screenshots under the game's name,
  PSP/SCREENSHOT too), delete a photo, and a photo editor (rotate, mirror, eight filters,
  brightness, contrast, saturation, warmth, vignette) that saves a copy.
- Pocketwatch Home in all three styles: Settings → Style now changes Home at once. Classic is
  the XMB itself - its sky and wave in your theme colour, the system's own category icons, the
  game list with the game's picture behind it, the option list with the options on the right,
  left / right for the categories. Modern got section pills, a gliding white bar, switches, a
  photo grid, a music card with what plays; Minimal is plain words.
- Pocketwatch Home: System starts with USB Connection, Internet Browser and Exit to the XMB.
- Pocketwatch Home: System → Internet Browser (the system's browser).
- Pocketwatch Home: key sounds of its own (made in code; Settings → Key Sounds).
- Pocketwatch Home: a game's Information (Triangle), a photo slideshow (Settings → Slideshow
  Speed) and a wallpaper from Photos (Square), shown behind every section but Games.
- Pocketwatch Home: games sorted by name or the last played first; PSN games' EBOOTs; the
  battery's voltage, temperature, capacity and charge cycles in System.
- Pocketwatch Home: Music (the MP3s of MUSIC/, playing on while you browse, the next track
  following) and Photos (PICTURE/ and DCIM/: thumbnails, the picture behind the row, full screen).
- Pocketwatch Home: a Settings section (Pocketwatch's settings, nickname, system language, date
  and time formats, time zone, summer time, WLAN power save, brightness, AVLS), System
  Information (model, firmware, ARK-4, MAC), and Triangle on a game: its save states with
  pictures - "continue from here" - and its Saved Data (the system's own list).
- Pocketwatch Home keeps its backgrounds in video memory: the GE no longer spends most of a
  frame reading them (45-49 fps measured before); opening System no longer freezes it for seconds.
- USB Connection in Pocketwatch Home (HOME panel or System): one press; it reconnects by itself
  when the computer does not take the card, and waits for the cable when there is none.
- Faster in-game menu: every style should now make 60 frames a second (Classic's wave renewed in
  quarters, the clock corner cached, the pixel helpers inlined).
- Pocketwatch Home is drawn by the PSP's GPU (the GE): smooth instead of laggy; its frame rate
  goes to `seplugins/pocketwatch/home.log`.
- HOME in Pocketwatch Home opens its own panel (Close, Exit to the XMB, Sleep, Turn Off) instead
  of the system's quit dialog.
- The PSP can start in Pocketwatch Home instead of the XMB: XMB → Settings → Pocketwatch → Start
  screen. Hold L while switching on for the XMB; Home's Exit and HOME → Quit go to the XMB once.
- Pocketwatch Home lists and starts homebrew and PS1 games from `PSP/GAME` too.
- Pocketwatch Home (`PSP/GAME/PocketwatchHome`): a home screen of its own in the Modern style -
  every game on the card with its own icon and background art, its play time and save states,
  Play Time and System sections - that starts games the way ARK-4's menu does.
- XMB → Settings → Battery: the charge, the status, the time left (or to full), voltage,
  current, temperature, capacity and charge cycles; estimates say what they rest on.
- XMB → Game → Play Time: every game's play time, most played first. Only time with Pocketwatch
  running in the game is counted, and none of the hub, saves, loads or sleeps.
- The hub has Resume Game, Save State, Load State and Quit Game; its settings moved to
  XMB → Settings → Pocketwatch.
- Settings → Pocketwatch in the XMB: the plugin's settings (style, fast save and load, clock and
  battery, language) in the XMB's own option list, from a second, small XMB plugin
  (`pocketwatch_xmb.prx`) that works alongside ARK-4's own XMB additions.
- A volume bar over the menu in each style: the level glides to the new value and the bar fades
  out after the last change (the system's own bar does not show over the menu).
- ARK-4's exit combos cannot fire in the middle of a save or load: Pocketwatch reads the buttons
  through a controller function ARK-4 does not hook.
- Continue a game from a state at its start: a `continue.txt` next to the plugin
  (`<disc ID> <slot>`) loads that slot as soon as the game runs, once; the XMB's "Continue from
  Save State" will write it.
- States on the Memory Stick: five slots per game as native saved data
  (`PSP/SAVEDATA/PW<DISCID><NN>`, hashed PARAM.SFO, the game's frame as ICON0/PIC1), listed and
  deletable in the XMB's Saved Data Utility; a state list in the Saved Data Utility's layout with
  overwrite and delete confirmations; loads after quitting the game or a full power-off.
- A state loads only into the setup that made it (disc ID, ISO path and size, the plugin file,
  model, memory layout); after every load from a slot the restored kernel checks that it sees the
  card as it is now before anything writes to it.
- Guards: save and load are refused, with the reason, on low battery without the AC adaptor, with
  USB mass storage on, with the wireless LAN in use, under ARK's high memory, and with Custom Home
  or PSPStates loaded. The hub survives a sleep with the POWER switch.
- HOME opens Pocketwatch's hub instead of the system HOME screen: Resume Game, Save State, Load
  State, Quit Game, in the XMB's look - the month's background and wave, the system font, the
  XMB's own icons and button pictures, clock and battery in the user's formats.
- Quit Game through the game's own exit path, with ARK-4's exit as the fallback.
- Savestate engine: freeze the game, pack game RAM / eDRAM / scratchpad with LZ4, capture the
  kernel during a suspend (touch-and-go after the first save of a session), roll it back on load
  with the flash drivers rebuilt and the IPL's seeds put back.
- Two more looks for the menu, chosen in Settings → Style: Modern (the game blurred behind round
  buttons, cards and switches) and Minimal (only text over the darkened game); Classic (XMB)
  stays the default.
- Settings in the hub: fast save and load on or off, the clock and battery corner, the language
  (any of the translations, or the system's); kept in `config.txt` / `language.txt`.
- The interface in the system language: French, Spanish, German, Italian, Dutch, Portuguese and
  Russian, plus Ukrainian through `language.txt`; translations are text files next to the plugin.
  Long dialog lines wrap like the system's.
- Release build switch (`scripts\build.cmd -Release`) and a device testing guide
  (`docs/testing.md`).
- Developer tooling: ARK-4 NID check, size budgets, host unit tests (ASan + UBSan), host previews
  of every screen, a documentation checker, debug combos, a hardware probe, a throughput benchmark
  and the T1 Memory Stick test.

### Changed
- Pocketwatch Home: text sits in the middle of every pill, row and button (by the font itself);
  lists, the settings' sidebar and the photo grids stop above the hints; the HOME panel darkens
  what is behind it; the settings' categories are no longer cut; long game names wrap on the
  launcher; settings are saved once you stop changing them, so stepping through them no longer
  stutters; a game's page opens at once.
- HOME opens the hub as it goes down, not when it is released, and the hub no longer reads the
  slots from the Memory Stick each time it opens.
- Relicensed from MIT to GPL-3.0-or-later.
- Registered with runlevel `UMD` (retail games only) instead of `GAME`, which includes homebrew.

### Fixed
- XMB → Settings → Pocketwatch: after changing the language, the rows' names showed broken text
  until the page was opened again.
- Pocketwatch Home: Classic's XMB icons and sky were missing in Home (the system refused the
  files to Home; they are read through the kernel now).
- Pocketwatch Home builds with the older PSPSDK too (`union SceNetApctlInfo`).
- USB Connection in Pocketwatch Home still failed when RemoteJoyLite (or another plugin with a USB
  device) was on for games: Home now takes the port from it for the connection and gives it back
  afterwards, as the XMB does.
- HOME in a game could freeze it for good (a game that reads its disc all the time, such as Need
  for Speed: Most Wanted): the game was stopped in the middle of a Memory Stick read and kept the
  card to itself. Pocketwatch now lets it finish the read first.
- USB Connection in Pocketwatch Home did not connect: it switched the connection off and on again
  every 3 s while the computer was still taking the card. It now connects once, as ARK-4's menu
  does; Connect Again (Enter) is there if the computer does not see the PSP.
- Pocketwatch Home stopped for about a third of a second every second on the PSP (30 fps or less,
  worse while changing settings): the battery and the system's settings are no longer read while
  it draws.
- Pocketwatch Home left only 512 KB of user memory to the firmware, not the 4 MB meant (a negative
  heap size takes all but libcglue's threshold): the keyboard, Saved Data, the MP3 decoder and the
  network could run out of memory on the PSP. Now 4 MB, and `home.log` shows the heap.
- Pocketwatch Home: a photo's name never showed in the viewer (its fade started at once).
- A state saved before Quit Game or a power-off was always refused as "saved with another
  setup": the check ran before the engine had measured its memory, so it compared with zeros.
- The menu ran at a very low frame rate: the background wave and the drawn shapes cost far less
  per frame now, and every movement runs by time instead of by frame.
- The state lists, dialogs and the option panel animate (glide, grow, fade, slide) like the
  system's; the Classic style uses the XMB's own battery picture (animated while charging) and the
  XMB's option-list look.
- Exiting a game hung on "Please wait...": `module_stop` now stops and joins all plugin threads.
- Quitting after a fast save hung on a black screen: the IPL's seeds are put back after every
  touch-and-go suspend.

[Unreleased]: https://github.com/semiloker/pocketwatch/compare/v0.1.0-alpha.1...HEAD
[0.1.0-alpha.1]: https://github.com/semiloker/pocketwatch/releases/tag/v0.1.0-alpha.1
