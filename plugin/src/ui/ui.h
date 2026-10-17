// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_UI_H
#define PW_UI_H

#include "gfx/image.h"
#include "gfx/text.h"
#include "ui/draw.h"
#include "ui/input.h"

/* What every screen of the menu shares: fonts, the system's button assignment, input pacing. */

#define PW_C_TEXT  PW_RGB(255, 255, 255)
#define PW_C_GREY  PW_RGB(192, 192, 192)
#define PW_C_BLACK PW_RGB(0, 0, 0)

/* The menu's look (docs/ui/styles.md): the XMB's own (default) or one of Pocketwatch's styles. */
enum { PW_STYLE_XMB, PW_STYLE_MODERN, PW_STYLE_MINIMAL, PW_STYLE_COUNT };

typedef struct {
    pw_font *title, *body;
    int style;                /* PW_STYLE_* */
    const pw_image *btn[4];   /* the system's X O triangle square pictures, or NULL (drawn) */
    const pw_image *battery;  /* the XMB's battery (4 frames, full to empty), or NULL (drawn) */
    const pw_image *focus;    /* the XMB's focus bar for text lists, or NULL */
    int cross_enter;          /* 1: X confirms (outside Japan), 0: O confirms */
    unsigned int enter, back; /* the matching PSP_CTRL_* bits */
    pw_keys keys;
    unsigned int t_tick;
} pw_ui;

void pw_ui_init(pw_ui *ui); /* fonts, Enter/Back from sceImposeGetLanguageMode */

/* Wait for the next vblank, keep the backlight on, return the buttons that went down now (with
   D-pad auto-repeat). */
unsigned int pw_ui_poll(pw_ui *ui);
void pw_ui_flush_input(pw_ui *ui); /* treat the buttons held now as already handled */
unsigned int pw_ui_buttons(void);
unsigned int pw_ui_now_ms(void);

#endif
