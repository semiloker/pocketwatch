# UI styles

The menu can be drawn in three styles, chosen in Settings → Style (`config.txt`: `style = xmb |
modern | minimal`). **Classic (XMB)** is the default and the reference for the native look
([UI spec](README.md)); the other two are Pocketwatch's own designs, added at the owner's request
(2026-10-17, [ADR-019](../architecture/decisions.md#adr-019--optional-ui-styles-next-to-the-native-look)).

Status: implemented, host previews reviewed; device review pending ([testing guide](../testing.md#settings), ST5).

| | Classic (XMB) | Modern | Minimal |
|---|---|---|---|
| Idea | the PSP's own HOME screen and XMB | a present-day console overlay (control-centre style) | nothing but text over the game |
| Backdrop | the month's XMB sky with the moving wave | the frozen game, blurred (box blur r=7, twice) and darkened to 47 %, darker towards the bottom | the frozen game darkened to 23 % |
| Hub | the XMB column: category icon, item icons (system RCO art), rows 65 px apart | the game's title top left; a dark band at the bottom with round buttons (drawn icons), a white disc that glides to the chosen one; its name and note above the band | the game's title top left (small); one centred column of names, the chosen one bright with a short rule under it, its note further down |
| Dialogs | between two white rules, Yes/No with a light box | a rounded card, pill buttons (white = chosen) | plain centred lines, Yes/No as words, the chosen one underlined |
| State list | the Saved Data Utility (icon column, details right) | a row of tiles, the chosen one large and framed, details on the band | the slots as lines on the left, the chosen one's picture on the right |
| Settings | the XMB option list, the option popup on the right | one card of rows, on/off as switches, the choices on their own card | "name … value" lines, the values stacked in place while choosing |
| Clock and battery | the XMB corner | a pill top right | small grey text top right |
| Hints | centred at the bottom, system button pictures | bottom right on the band | centred, quieter |
| Sony assets | font, icons, sky, button pictures (read from the PSP) | font and button pictures only | font and button pictures only |

## Implementation

- `ui/ui.h` `PW_STYLE_*`, `pw_ui.style` (from the config at every hub opening).
- Backdrops: [`ui/backdrop.c`](../../plugin/src/ui/backdrop.c) (portable, uses `pw_draw_blur` /
  `pw_draw_darken`), made by `ui/fb.c` when the menu opens or Settings changes the style
  (`pw_fb_set_style`).
- Screens: every `pw_view_*` in [`ui/views.c`](../../plugin/src/ui/views.c) hands over to
  [`views_modern.c`](../../plugin/src/ui/views_modern.c) or
  [`views_minimal.c`](../../plugin/src/ui/views_minimal.c) with the same parameters, so the
  complications and the hub do not know which style draws them.
- New view data both styles use: `pw_hub_cat.subtitle` (the game's title), `pw_setting_row.toggle`
  (on/off switches).
- Host previews: `make -C tests preview ...` writes every screen three times
  (`<screen>.png`, `modern-<screen>.png`, `minimal-<screen>.png`).

## Speed and motion (2026-10-17)

The owner reported a very low frame rate in Classic and Modern. `make -C tests bench` times a
frame by part on the host (only the proportions mean anything; the PSP is ~50x slower):

| Style | before (us) | after (us) | what changed |
|---|---|---|---|
| Classic | 680 | 343 | the wave: tables instead of two divisions a pixel, row order instead of column order (every pixel was a cache miss on the PSP) |
| Modern | 824 | 234 | shapes: the square root only on the one-pixel edge, discs on their own path, a triangle's edge lengths once |
| Minimal | 69 | 53 | blits: no division by 255 per pixel, two channels per multiply |

First device numbers after that change (2026-10-17,
[log](../research/logs/2026-10-17-hub-fps-castlevania-persona.log)): Modern 19 and 30 fps with
17-19 ms of drawing, Classic 11-16 fps with 18-21 ms. Two findings: a frame still took more than
the 16.7 ms of one vblank, and a loop took far longer than its drawing plus one vblank (Castlevania
lost 25-40 ms a loop elsewhere; Persona did not). So:

- Classic renews half of the wave per frame in a page of its own (the crest moves less than a
  pixel a frame, no seam shows); the drawn icons and the round buttons are rasterised once into a
  cache (`pw_icon_cache`, 64 KB next to the screen pages, dropped after every suspend).
- The hub draws only when something changed (the cursor moves, a fade, the clock or battery, a
  button, or Classic's wave): a still Modern or Minimal screen costs nothing.
- The log now splits a frame (backdrop, content, present, the wait for the vblank) and says which
  threads used the CPU while the hub was open (`cpu: hub, ... ms: <thread> NN% ...`, the idle
  threads included): the next run shows where Castlevania's missing time goes.

Motion follows the XMB's curve by time (an exponential with a 50 ms constant, "200 ms": VitaXMB's
reading of the PSP's engine), so a slower frame no longer slows the movement down. The state
list glides and grows the chosen icon, its details cross-fade; dialogs fade in (150 ms); the
Classic option panel slides in from the right. The hub logs its frame rate when it closes.

## The volume bar (UI-REQ-016)

Drawn over every frame of every screen (`pw_fb_present` calls the overlay that `pw_ui_init`
sets), fed from `sceImposeGetParam` each frame (`ui/volume.c`, portable; `ui/ui.c` reads the
system). Classic: a dark strip at the bottom with a speaker and 30 steps, the step being reached
filling in. Modern: a pill on the right edge, filled from the bottom, that swells for a moment at
each change. Minimal: a line above the hints and the number (`--` when muted). Previews:
`hub-volume.png`, `hub-volume-muted.png` per style.

## Adding a style

Add a `PW_STYLE_*` value and its name in `core/config.c`, a `views_<name>.c` implementing the seven
functions of [`views_style.h`](../../plugin/src/ui/views_style.h), its backdrop case, the dispatch
lines in `views.c`, a `style_<name>` string in every lang file, and the previews. The hub and the
complications stay as they are.

## Open points

- Modern: the band and the cards are drawn every frame over a static backdrop; the blur costs
  about 2 x 4 passes over 130 k pixels once per opening (RESEARCH REQUIRED: time it on the device,
  the log does not measure it yet).
- Whether the owner wants Modern's buttons to show the XMB's own icons instead of the drawn ones
  (OPEN QUESTION).
