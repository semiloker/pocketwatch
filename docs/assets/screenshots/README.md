# Screenshots

Real screenshots of Pocketwatch Home, taken in PPSSPP v1.19.3 on 2026-10-29 from the build of
[#11](https://github.com/semiloker/pocketwatch/issues/11) (`6734b4c`), at the PSP's own 480 x 272.
They show the README's screens ([#12](https://github.com/semiloker/pocketwatch/issues/12)).

| File | Style | Shows |
|---|---|---|
| [home-music.png](home-music.png) | Modern | Music: the track playing with its cover, the library |
| [home-player.png](home-player.png) | Modern | Now Playing: cover, time bar, the keys under the buttons |
| [home-photos.png](home-photos.png) | Modern | Photos: an album's grid |
| [home-editor.png](home-editor.png) | Modern | The photo editor: Adjust, warmth +40, the histogram |
| [home-settings.png](home-settings.png) | Modern | Settings in two panes |
| [home-classic.png](home-classic.png) | Classic | Music in the XMB's own layout, the wave over the month's sky |
| [home-minimal.png](home-minimal.png) | Minimal | Music as a plain list |

**What is in them, and whose it is:** only Pocketwatch's own drawing and demo content made for
these shots. The photos are drawn from gradients and shapes, the tracks are synthesized tones,
and their names ("Neon Rain", "Synthwave Trio", ...) are the fictional ones Home's previews use.
No game, game icon or other third-party file was on PPSSPP's memory stick. The text is PPSSPP's
own replacement of the PSP's font (PPSSPP ships open fonts in its `flash0`, not Sony's), and Home
found none of the system's pictures there (`home.log`: `0 system picture(s)`). On a real PSP,
Classic draws the XMB's own icons from flash0 instead of Pocketwatch's.

**Not shown:** the brightness bar. In PPSSPP Home can neither read nor set the backlight
(those calls go through ARK-4), so the bar never appears there. Its look is in the host
previews (`make -C tests preview`, `home-osd.png` per style).

Retake them after a visible change: PPSSPP with `home\EBOOT.PBP`, demo content of your own on its
memory stick, never game icons or Sony files.
