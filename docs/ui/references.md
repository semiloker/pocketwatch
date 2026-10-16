# UI visual reference library

Real products and implementations to study for the native look. **None of these is Pocketwatch**,
and no image from them is stored in the repository: Sony's UI is copyrighted and most screenshots
have unclear licenses, so the library links to sources ([ADR-013](../architecture/decisions.md#adr-013--sony-assets-are-read-at-runtime-never-redistributed)). Device captures made for
comparison are kept outside the repository. Accessed 2026-10-16.

| # | Reference | Source | What to learn | Adopt / adapt | Do not copy | License |
|---|---|---|---|---|---|---|
| 1 | The real PSP-3000 on 6.61 (owner's device) | debug framebuffer capture (`sceDisplayGetFrameBuf`) of the HOME screen, XMB, Saved Data Utility, message dialogs | exact pixels, colours, timing | the measuring baseline for every metric | the captures themselves (never committed or shared) | © Sony |
| 2 | PPSSPP "Load the VSH" | <https://www.ppsspp.org/docs/getting-started/how-to-run-the-vsh/> | the real 6.61 XMB at exact 1x on a PC, for measuring and frame-stepping animations | measurements | firmware, captures | PPSSPP GPL-2.0+; firmware © Sony |
| 3 | PPSSPP Saved Data dialog | [PSPSaveDialog.cpp L380-L663](https://github.com/hrydgard/ppsspp/blob/master/Core/Dialog/PSPSaveDialog.cpp#L380-L663) | Saved Data Utility layout constants (icon column, details pane, banner) | layout numbers for the state list | PPGe atlas art | GPL-2.0+ |
| 4 | PPSSPP message dialog | [PSPDialog.cpp L435-L560](https://github.com/hrydgard/ppsspp/blob/master/Core/Dialog/PSPDialog.cpp#L435-L560) | dim level, fades, white rules, Yes/No placement, hint bar | dialog widget | — | GPL-2.0+ |
| 5 | VitaXMB | <https://github.com/dekiller82/VitaXMB> | XMB geometry and timings measured from 6.60/6.61, RCO handling, generated glow | theme constants (to re-verify), glow approach | `assets/font.otf`, extracted PSP images | code GPL-3.0 |
| 6 | ARK-4 VSH menu | <https://github.com/PSP-Archive/ARK-4/tree/main/extras/menus/vshmenu> | overlay structure, registry access (button swap) | registry reading | its non-XMB styling | GPL-3.0+ |
| 7 | Adrenaline menu | <https://github.com/TheOfficialFloW/Adrenaline/blob/master/user/menu.c> | using the system PGF for a native feel | the idea | its colours (Vita-styled) | GPL-3.0 |
| 8 | Custom Home (CustomHOME) | <https://www.gamebrew.org/wiki/CustomHOME_PSP>, screenshot <https://www.gamebrew.org/wiki/File:Customhome.png> | precedent for an in-game HOME replacement, item set | behaviour ideas | look, code (license unclear) | unclear — link only |
| 9 | Sony PSP User's Guide | <https://manuals.playstation.net/document/en/psp/current/> | official wording ("Saved Data Utility", "Quit Game"), HOME behaviour | string wording | text verbatim beyond short UI labels | © Sony — link only |
| 10 | XrossMediaBar (Wikipedia) | <https://en.wikipedia.org/wiki/XrossMediaBar> | the cross concept (categories × items) shared by PSP and PS3 | Concept B rationale | the non-free screenshot | article CC BY-SA; image non-free |
| 11 | PS3 Dev wiki — XMB fonts | <https://www.psdevwiki.com/ps3/XMB_Fonts> | button code points in imagefont.bin (PS3) | background only | — | unclear — link only |
| 12 | PPSSPP "Using original fonts" | <https://www.ppsspp.org/docs/getting-started/using-original-fonts> | which files make up the font set | file list | — | PPSSPP docs |
| 13 | JPCSP font info | [SceFontInfo.java](https://github.com/jpcsp/jpcsp/blob/master/src/jpcsp/HLE/kernel/types/SceFontInfo.java) | shadow and overlay glyph decoding | decoder logic | — | GPL-3.0+ |
| 14 | techenclave PSP UI guide | <https://techenclave.com/t/guide-to-customize-psp-ui/86287> | evidence that the XMB uses ltn0.pgf | — | its flash-writing steps (dangerous) | unclear — link only |

How to use: every metric in the theme table cites the reference it came from; a value from a
reimplementation (5) is **INFERRED** until checked against (1) or (2).
