// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * The modern style (docs/ui/styles.md): the frozen game softened behind everything, a dark band at
 * the bottom with round buttons (the cursor is a white disc that glides between them), rounded
 * cards for dialogs and settings, the state list as a row of tiles. Inspired by today's console
 * overlays; Pocketwatch's own drawing, no system pictures except the button symbols.
 */
#include "ui/i18n.h"
#include "ui/icons.h"
#include "ui/shapes.h"
#include "ui/views_style.h"

#define SCREEN_W 480
#define SCREEN_H 272

#define C_TEXT PW_RGB(255, 255, 255)
#define C_SUB  PW_RGB(176, 182, 196)
#define C_CARD PW_RGB(28, 30, 40)
#define C_BAND PW_RGB(14, 15, 20)
#define C_KNOB PW_RGB(70, 75, 92)
#define C_ON   PW_RGB(0, 114, 206)
#define C_INK  PW_RGB(16, 17, 22) /* text and icons on white */

#define BAND_Y  192
#define BTN_Y   230
#define BTN_R   20
#define SEL_R   24
#define BTN_GAP 62

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static unsigned int blend(unsigned int a, unsigned int b, int t) /* a..b, t 0-256 */
{
    unsigned int r = ((a & 0xFF) * (unsigned int)(256 - t) + (b & 0xFF) * (unsigned int)t) >> 8;
    unsigned int g =
        (((a >> 8) & 0xFF) * (unsigned int)(256 - t) + ((b >> 8) & 0xFF) * (unsigned int)t) >> 8;
    unsigned int bl =
        (((a >> 16) & 0xFF) * (unsigned int)(256 - t) + ((b >> 16) & 0xFF) * (unsigned int)t) >> 8;

    return PW_RGB(r, g, bl);
}

static void rrect(pw_canvas *c, int x, int y, int w, int h, int r, unsigned int color, int alpha)
{
    pw_shape_rrect(c, PW_FX(x), PW_FX(y), PW_FX(w), PW_FX(h), PW_FX(r), 0, color, alpha);
}

static void text_center(pw_canvas *c, pw_font *f, int cx, int y, const char *s, int max,
                        unsigned int color, int alpha)
{
    int w = pw_text_width(f, s);

    if (w > max)
        w = max;
    pw_text_draw_fit(c, f, cx - w / 2, y, s, max, color, alpha, 0);
}

void pw_modern_hints(pw_canvas *c, const pw_ui *ui, const char *enter_label, const char *back_label,
                     int alpha)
{
    /* the right edge of the band: X first, then O, like the console's own hints */
    const char *cross = ui->cross_enter ? enter_label : back_label;
    const char *circle = ui->cross_enter ? back_label : enter_label;
    int x = SCREEN_W - 14;

    if (circle) {
        x -= pw_text_width(ui->body, circle);
        pw_text_draw(c, ui->body, x, 264, circle, C_SUB, alpha, 0);
        pw_view_button(c, ui, PW_BTN_CIRCLE, x - 9, 260, alpha);
        x -= 26;
    }
    if (cross) {
        x -= pw_text_width(ui->body, cross);
        pw_text_draw(c, ui->body, x, 264, cross, C_SUB, alpha, 0);
        pw_view_button(c, ui, PW_BTN_CROSS, x - 9, 260, alpha);
    }
}

void pw_modern_status(pw_canvas *c, const pw_ui *ui, const char *clock, int battery, int charging,
                      int alpha, unsigned int t_ms)
{
    (void)t_ms;
    int tw = clock && clock[0] ? pw_text_width(ui->body, clock) : 0;
    int w = tw + (battery >= 0 ? 34 : 0) + 20, x = SCREEN_W - 12 - w;

    if (w <= 20)
        return;
    rrect(c, x, 8, w, 20, 10, C_CARD, alpha * 190 / 256);
    if (tw)
        pw_text_draw(c, ui->body, x + 10, 22, clock, C_TEXT, alpha, 0);
    if (battery >= 0) { /* a small cell, filled to the level */
        int bx = x + w - 30, fill = 14 * clampi(battery, 0, 100) / 100;

        pw_shape_rrect(c, PW_FX(bx), PW_FX(13), PW_FX(18), PW_FX(10), PW_FX(2), PW_FX(1) + 4,
                       C_TEXT, alpha);
        pw_draw_blend(c, bx + 18, 16, 2, 4, C_TEXT, alpha);
        pw_draw_blend(c, bx + 2, 15, fill, 6, charging ? PW_RGB(120, 220, 120) : C_TEXT, alpha);
    }
}

void pw_modern_hub(pw_canvas *c, const pw_ui *ui, const pw_hub_cat *cat, const pw_hub_row *rows,
                   int count, int selpos, int fade)
{
    int x0 = SCREEN_W / 2 - (count - 1) * BTN_GAP / 2,
        sel = clampi((selpos + 128) / 256, 0, count - 1);

    if (cat->subtitle) /* the game, top left, like a card's title */
        pw_text_draw_fit(c, ui->title, 20, 32, cat->subtitle, 280, C_TEXT, fade, 0);

    pw_draw_blend(c, 0, BAND_Y, SCREEN_W, SCREEN_H - BAND_Y, C_BAND, fade * 150 / 256);
    pw_draw_blend(c, 0, BAND_Y, SCREEN_W, 1, C_TEXT, fade * 30 / 256);

    for (int i = 0; i < count; i++) /* the buttons' faces */
        pw_icon_draw(c, PW_ICON_DISC, x0 + i * BTN_GAP, BTN_Y, 2 * BTN_R, C_KNOB, fade * 170 / 256);
    if (count > 0) /* the cursor glides between them */
        pw_icon_draw(c, PW_ICON_DISC, x0 + (selpos * BTN_GAP + 128) / 256, BTN_Y, 2 * SEL_R, C_TEXT,
                     fade);
    for (int i = 0; i < count; i++) {
        int d = i * 256 - selpos, near = 256 - clampi(d < 0 ? -d : d, 0, 256);
        int alpha = rows[i].enabled ? fade : fade * 110 / 256;

        pw_icon_draw(c, rows[i].fallback, x0 + i * BTN_GAP, BTN_Y, 24, blend(C_TEXT, C_INK, near),
                     alpha);
    }

    if (count > 0) { /* the chosen one's name above the band */
        const pw_hub_row *r = &rows[sel];

        text_center(c, ui->title, SCREEN_W / 2, BAND_Y - 20, r->title, 420,
                    r->enabled ? C_TEXT : C_SUB, fade);
        if (r->desc)
            text_center(c, ui->body, SCREEN_W / 2, BAND_Y - 5, r->desc, 420, C_SUB, fade);
    }
    pw_modern_hints(c, ui, pw_tr(PW_S_ENTER), pw_tr(PW_S_BACK), fade);
}

static void pill_button(pw_canvas *c, const pw_ui *ui, int cx, int y, const char *label,
                        int selected, int alpha)
{
    int w = pw_text_width(ui->title, label) + 36;

    if (w < 96)
        w = 96;
    rrect(c, cx - w / 2, y, w, 26, 13, C_TEXT, (selected ? 256 : 40) * alpha / 256);
    text_center(c, ui->title, cx, y + 18, label, w - 12, selected ? C_INK : C_TEXT, alpha);
}

void pw_modern_dialog(pw_canvas *c, const pw_ui *ui, int kind, const char *line1, const char *line2,
                      int yes, int appear)
{
    char lines[4][PW_VIEW_LINE_MAX];
    int n = pw_view_wrap(ui->title, line1, 270, lines, 0, line2 ? 2 : 3), h, y;
    int a = appear < 0 ? 0 : appear > 256 ? 256 : appear;

    if (line2)
        n = pw_view_wrap(ui->title, line2, 270, lines, n, n + 2 > 4 ? 4 : n + 2);
    h = 30 + n * 18 + 46;
    y = SCREEN_H / 2 - h / 2 - 6 + (256 - a) * 12 / 256; /* the card rises into place */
    pw_draw_blend(c, 0, 0, SCREEN_W, SCREEN_H, PW_RGB(0, 0, 0), 90 * a / 256);
    rrect(c, 80, y, 320, h, 14, C_CARD, 240 * a / 256);
    for (int i = 0; i < n; i++)
        text_center(c, ui->title, SCREEN_W / 2, y + 32 + i * 18, lines[i], 290,
                    i == 0 ? C_TEXT : C_SUB, a);
    if (kind == PW_VIEW_CONFIRM) {
        pill_button(c, ui, SCREEN_W / 2 - 64, y + h - 40, pw_tr(PW_S_YES), yes, a);
        pill_button(c, ui, SCREEN_W / 2 + 64, y + h - 40, pw_tr(PW_S_NO), !yes, a);
        pw_modern_hints(c, ui, pw_tr(PW_S_ENTER), pw_tr(PW_S_BACK), a);
    } else {
        pill_button(c, ui, SCREEN_W / 2, y + h - 40, pw_tr(PW_S_OK), 1, a);
        pw_modern_hints(c, ui, pw_tr(PW_S_ENTER), 0, a);
    }
}

void pw_modern_busy(pw_canvas *c, const pw_ui *ui, const char *text, unsigned int t_ms)
{
    int cx = SCREEN_W / 2, cy = 118, head = (int)(t_ms / 80) % 12;

    pw_draw_blend(c, 0, 0, SCREEN_W, SCREEN_H, PW_RGB(0, 0, 0), 70);
    for (int i = 0; i < 12; i++) { /* a ring of dots, the bright one chasing round */
        static const signed char k[12][2] = {{0, -16}, {8, -14}, {14, -8},  {16, 0},
                                             {14, 8},  {8, 14},  {0, 16},   {-8, 14},
                                             {-14, 8}, {-16, 0}, {-14, -8}, {-8, -14}};
        int age = (head - i + 12) % 12;

        pw_shape_disc(c, PW_FX(cx + k[i][0]), PW_FX(cy + k[i][1]), PW_FX(2) + 4, C_TEXT,
                      256 - age * 20);
    }
    text_center(c, ui->title, cx, 162, text, 440, C_TEXT, 256);
}

/* The state list: a row of tiles, the chosen one large in the middle with its details below. */
#define TILE_W  144
#define TILE_H  80
#define SMALL_W 104
#define SMALL_H 58
#define TILE_CY 112
#define TILE_DX 132

static void tile(pw_canvas *c, const pw_slot_row *r, int cx, int cy, int w, int h, int alpha)
{
    int x = cx - w / 2, y = cy - h / 2;

    if (x + w < 0 || x > SCREEN_W)
        return;
    if (r->exists && r->icon) {
        pw_draw_image_scaled(c, r->icon, 144, 80, x, y, w, h, alpha);
    } else {
        rrect(c, x, y, w, h, 6, C_CARD, alpha * 200 / 256);
        pw_shape_capsule(c, PW_FX(cx - 10), PW_FX(cy), PW_FX(cx + 10), PW_FX(cy), PW_FX(1) + 4,
                         C_SUB, alpha);
        pw_shape_capsule(c, PW_FX(cx), PW_FX(cy - 10), PW_FX(cx), PW_FX(cy + 10), PW_FX(1) + 4,
                         C_SUB, alpha);
    }
}

static int nearness(int p)
{
    return p >= 256 || p <= -256 ? 0 : 256 - (p < 0 ? -p : p);
}

void pw_modern_slots(pw_canvas *c, const pw_ui *ui, const char *heading, const char *game,
                     const pw_slot_row *rows, int count, int selpos, int can_delete, int appear)
{
    int sel = clampi((selpos + 128) / 256, 0, count - 1), off = sel * 256 - selpos;
    int dn = 256 - 2 * (off < 0 ? -off : off), text_a = (dn < 0 ? 0 : dn) * appear / 256;
    int cy = TILE_CY + (256 - appear) * 14 / 256; /* the row rises into place */
    const pw_slot_row *r = &rows[sel];

    pw_text_draw_fit(c, ui->title, 20, 32, heading, 300, C_TEXT, appear, 0);
    if (game)
        pw_text_draw_fit(c, ui->body, 20, 48, game, 300, C_SUB, appear, 0);
    for (int i = 0; i < count; i++) { /* the tiles glide; the one in the middle grows */
        int p = i * 256 - selpos, near = nearness(p);

        tile(c, &rows[i], SCREEN_W / 2 + p * TILE_DX / 256, cy,
             SMALL_W + (TILE_W - SMALL_W) * near / 256, SMALL_H + (TILE_H - SMALL_H) * near / 256,
             (150 + 106 * near / 256) * appear / 256);
    }
    pw_shape_rrect(c, PW_FX(SCREEN_W / 2 - TILE_W / 2 - 3), PW_FX(cy - TILE_H / 2 - 3),
                   PW_FX(TILE_W + 6), PW_FX(TILE_H + 6), PW_FX(5), PW_FX(2), C_TEXT, appear);

    pw_draw_blend(c, 0, BAND_Y, SCREEN_W, SCREEN_H - BAND_Y, C_BAND, 150);
    text_center(c, ui->title, SCREEN_W / 2, 172, r->title, 440, C_TEXT, text_a);
    if (r->info)
        text_center(c, ui->body, SCREEN_W / 2, 214, r->info, 440, C_SUB, text_a);
    pw_modern_hints(c, ui, pw_tr(PW_S_ENTER), pw_tr(PW_S_BACK), 256);
    if (can_delete && r->exists) {
        pw_view_button(c, ui, PW_BTN_TRIANGLE, 22, 260, 256);
        pw_text_draw(c, ui->body, 32, 264, pw_tr(PW_S_DELETE), C_SUB, 256, 0);
    }
}

/* A pill on the right edge, filled from the bottom, that swells a little at each change; the
   brightness's on the left edge with a sun (#9). */
void pw_modern_volume(pw_canvas *c, const pw_ui *ui, const pw_volume *v)
{
    int sun = v->max > PW_VOLUME_MAX, max = sun ? v->max : PW_VOLUME_MAX, a = v->alpha,
        bump = v->flash * 3 / 256, w = 22 + 2 * bump, h = 136 + 2 * bump,
        x = sun ? 26 - w / 2 : SCREEN_W - 26 - w / 2, y = 68 - bump, cx = x + w / 2, top = y + 14,
        bottom = y + h - 36, level = bottom - (bottom - top) * v->shown / (max * 256);

    (void)ui;
    if (a <= 0)
        return;
    rrect(c, x, y, w, h, w / 2, C_CARD, a * 210 / 256);
    pw_shape_capsule(c, PW_FX(cx), PW_FX(top), PW_FX(cx), PW_FX(bottom), PW_FX(3), C_KNOB, a);
    if (v->shown > 0)
        pw_shape_capsule(c, PW_FX(cx), PW_FX(level), PW_FX(cx), PW_FX(bottom), PW_FX(3),
                         v->muted ? C_SUB : C_TEXT, a);
    if (sun)
        pw_view_sun(c, cx, y + h - 20, C_TEXT, a);
    else
        pw_view_speaker(c, cx - 8, y + h - 28, v->muted, C_TEXT, a);
}
