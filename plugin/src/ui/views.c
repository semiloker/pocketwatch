// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views.h"
#include "ui/icons.h"
#include "ui/shapes.h"

#define SCREEN_W 480
#define SCREEN_H 272

/* Hub: the XMB's item column (INFERRED metrics from VitaXMB, to be checked on the device). */
#define CAT_X     109
#define CAT_Y     70
#define ICON_X    104
#define TEXT_X    150
#define SEL_Y     135
#define ROW_PITCH 34
#define ICON_SEL  52
#define ICON_ROW  30
#define ROW_ALPHA 208
#define TEXT_MAX  (SCREEN_W - TEXT_X - 6)

/* Dialog: PPSSPP's message dialog layout (PSPDialog.cpp). */
#define RULE_X0  60
#define RULE_X1  420
#define RULE_TOP 98
#define RULE_BOT 177
#define TEXT_Y1  126
#define TEXT_Y2  144
#define CHOICE_Y 165
#define YES_X    204
#define NO_X     273

void pw_view_text_centered(pw_canvas *c, pw_font *f, int cx, int y, const char *s,
                           unsigned int color, int alpha)
{
    int w = pw_text_width(f, s), max = RULE_X1 - RULE_X0 + 20;

    if (w > max)
        w = max;
    pw_text_draw_fit(c, f, cx - w / 2, y, s, max, color, alpha, 1);
}

void pw_view_hints(pw_canvas *c, const pw_ui *ui, const char *enter_label, const char *back_label,
                   int alpha)
{
    /* The PSP always shows X left of O and swaps only what they mean (PPSSPP PSPDialog.cpp). */
    const char *cross = ui->cross_enter ? enter_label : back_label;
    const char *circle = ui->cross_enter ? back_label : enter_label;

    if (cross) {
        pw_button_draw(c, PW_BTN_CROSS, 189, 256, 12, PW_C_TEXT, alpha);
        pw_text_draw(c, ui->body, 199, 260, cross, PW_C_TEXT, alpha, 1);
    }
    if (circle) {
        pw_button_draw(c, PW_BTN_CIRCLE, 267, 256, 12, PW_C_TEXT, alpha);
        pw_text_draw(c, ui->body, 277, 260, circle, PW_C_TEXT, alpha, 1);
    }
}

/* Row centre for a position relative to the cursor (1/256 rows): the selected row at SEL_Y, the
   rows after it below, the one before it above the category icon, like the XMB. */
static int row_y(int p)
{
    if (p >= 256)
        return SEL_Y + 19 + ROW_PITCH * p / 256;
    if (p >= 0)
        return SEL_Y + (19 + ROW_PITCH) * p / 256;
    if (p >= -256)
        return SEL_Y + (SEL_Y - (CAT_Y - 52)) * p / 256;
    return (CAT_Y - 52) + ROW_PITCH * (p + 256) / 256;
}

void pw_view_hub(pw_canvas *c, const pw_ui *ui, const pw_hub_row *rows, int count, int selpos,
                 int fade)
{
    pw_icon_draw(c, PW_ICON_WATCH, CAT_X, CAT_Y, 56, PW_C_TEXT, fade);
    for (int i = 0; i < count; i++) {
        const pw_hub_row *r = &rows[i];
        int p = i * 256 - selpos, ap = p < 0 ? -p : p, y = row_y(p);
        int near = ap >= 256 ? 0 : 256 - ap; /* 256 on the cursor, 0 one row away */
        int size = ICON_ROW + (ICON_SEL - ICON_ROW) * near / 256;
        int alpha = ROW_ALPHA + (256 - ROW_ALPHA) * near / 256;

        if (y < -40 || y > SCREEN_H + 40)
            continue;
        if (!r->enabled)
            alpha /= 2;
        alpha = alpha * fade / 256;
        pw_icon_draw(c, r->icon, ICON_X, y, size, PW_C_TEXT, alpha);
        if (near > 128) {
            pw_text_draw_fit(c, ui->title, TEXT_X, y + 1, r->title, TEXT_MAX, PW_C_TEXT, alpha, 1);
            if (r->desc)
                pw_text_draw_fit(c, ui->body, TEXT_X, y + 17, r->desc, TEXT_MAX, PW_C_TEXT,
                                 alpha * (near - 128) / 128, 1);
        } else {
            pw_text_draw_fit(c, ui->title, TEXT_X, y + 5, r->title, TEXT_MAX, PW_C_TEXT, alpha, 1);
        }
    }
}

static void choice(pw_canvas *c, const pw_ui *ui, int x, const char *label, int selected)
{
    int w = pw_text_width(ui->title, label), bw = w + 8 > 30 ? w + 8 : 30;

    if (selected) /* PPSSPP's 30x16 box in 0x6DCFCFCF */
        pw_draw_blend(c, x - bw / 2, CHOICE_Y - 12, bw, 16, PW_RGB(0xCF, 0xCF, 0xCF), 110);
    pw_text_draw(c, ui->title, x - w / 2, CHOICE_Y, label, PW_C_TEXT, 256, 1);
}

void pw_view_dialog(pw_canvas *c, const pw_ui *ui, int kind, const char *line1, const char *line2,
                    int yes)
{
    pw_draw_blend(c, 0, 0, SCREEN_W, SCREEN_H, PW_C_BLACK, 64);
    pw_draw_fill(c, RULE_X0, RULE_TOP, RULE_X1 - RULE_X0, 1, PW_C_TEXT);
    pw_draw_fill(c, RULE_X0, RULE_BOT, RULE_X1 - RULE_X0, 1, PW_C_TEXT);
    if (line2) {
        pw_view_text_centered(c, ui->title, SCREEN_W / 2, TEXT_Y1, line1, PW_C_TEXT, 256);
        pw_view_text_centered(c, ui->title, SCREEN_W / 2, TEXT_Y2, line2, PW_C_TEXT, 256);
    } else {
        pw_view_text_centered(c, ui->title, SCREEN_W / 2, (TEXT_Y1 + TEXT_Y2) / 2, line1, PW_C_TEXT,
                              256);
    }
    if (kind == PW_VIEW_CONFIRM) {
        choice(c, ui, YES_X, "Yes", yes);
        choice(c, ui, NO_X, "No", !yes);
        pw_view_hints(c, ui, "Enter", "Back", 256);
    } else {
        choice(c, ui, SCREEN_W / 2, "OK", 1);
        pw_view_hints(c, ui, "Enter", 0, 256);
    }
}

void pw_view_busy(pw_canvas *c, const pw_ui *ui, const char *text)
{
    static const signed char k[8][2] = {{0, -12}, {8, -8}, {12, 0},  {8, 8},
                                        {0, 12},  {-8, 8}, {-12, 0}, {-8, -8}};
    int cx = SCREEN_W / 2, cy = 120;

    for (int i = 0; i < 8; i++) /* the busy ring: eight dots, fading round */
        pw_shape_disc(c, PW_FX(cx + k[i][0]), PW_FX(cy + k[i][1]), PW_FX(2) + 6, PW_C_TEXT,
                      256 - i * 26);
    pw_view_text_centered(c, ui->title, cx, 158, text, PW_C_TEXT, 256);
}
