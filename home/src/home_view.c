// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "home_view.h"
#include "ui/icons.h"
#include "ui/shapes.h"

#define C_TEXT PW_RGB(255, 255, 255)
#define C_SUB  PW_RGB(176, 182, 196)
#define C_CARD PW_RGB(28, 30, 40)
#define C_KNOB PW_RGB(70, 75, 92)
#define C_ON   PW_RGB(0, 114, 206)
#define C_INK  PW_RGB(16, 17, 22)

#define ROW_TOP   56 /* the icons' top edge */
#define SEL_X     24 /* the chosen icon's left edge */
#define SEL_GAP   14 /* after the chosen icon */
#define SMALL_W   96 /* the others */
#define SMALL_H   53
#define SMALL_GAP 10
#define TITLE_Y   176
#define META_Y    196
#define PLAY_Y    210
#define CARD_X    40
#define CARD_Y    54
#define CARD_W    400
#define ROW_H     28
#define SHOW      7

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static int lerp(int a, int b, int t) /* t 0-256 */
{
    return a + (b - a) * t / 256;
}

static void rrect(pw_canvas *c, int x, int y, int w, int h, int r, unsigned int color, int alpha)
{
    pw_shape_rrect(c, PW_FX(x), PW_FX(y), PW_FX(w), PW_FX(h), PW_FX(r), 0, color, alpha);
}

static void text_right(pw_canvas *c, pw_font *f, int x1, int y, const char *s, unsigned int color,
                       int alpha)
{
    pw_text_draw(c, f, x1 - pw_text_width(f, s), y, s, color, alpha, 0);
}

/* No art: a slate gradient with a faint glow of the accent at the top. */
static void gradient(pw_canvas *c)
{
    for (int y = 0; y < HM_H; y++) {
        int t = y * 256 / HM_H;

        pw_draw_fill(c, 0, y, HM_W, 1, PW_RGB(lerp(26, 8, t), lerp(29, 9, t), lerp(42, 14, t)));
    }
    pw_shape_disc(c, PW_FX(360), PW_FX(-40), PW_FX(150), C_ON, 30);
}

static void backdrop(pw_canvas *c, const hm_view *v)
{
    if (v->bg && v->bg_prev && v->bg_mix < 256) {
        pw_draw_lerp(c, v->bg_prev, v->bg, v->bg_mix);
        return;
    }
    if (v->bg) {
        for (int y = 0; y < HM_H; y++)
            memcpy(c->px + y * c->stride, v->bg + y * c->stride, HM_W * 4);
        return;
    }
    gradient(c);
}

/* The clock and battery pill at the top right; returns its left edge. */
static int status(pw_canvas *c, const hm_ui *ui, const hm_view *v, int a)
{
    int tw = v->clock && v->clock[0] ? pw_text_width(ui->body, v->clock) : 0;
    int w = tw + (v->battery >= 0 ? 34 : 0) + 20, x = HM_W - 12 - w;

    if (w <= 20)
        return HM_W;
    rrect(c, x, 8, w, 20, 10, C_CARD, a * 190 / 256);
    if (tw)
        pw_text_draw(c, ui->body, x + 10, 22, v->clock, C_TEXT, a, 0);
    if (v->battery >= 0) {
        int bx = x + w - 30, fill = 14 * clampi(v->battery, 0, 100) / 100;

        pw_shape_rrect(c, PW_FX(bx), PW_FX(13), PW_FX(18), PW_FX(10), PW_FX(2), PW_FX(1) + 4,
                       C_TEXT, a);
        pw_draw_blend(c, bx + 18, 16, 2, 4, C_TEXT, a);
        pw_draw_blend(c, bx + 2, 15, fill, 6, v->charging ? PW_RGB(120, 220, 120) : C_TEXT, a);
    }
    return x;
}

/* The section names at the top left; a pill glides under the chosen one. */
static void tabs(pw_canvas *c, const hm_ui *ui, const hm_view *v, int a, int limit)
{
    int x[HM_TABS + 1], w[HM_TABS], from, to, f;
    pw_font *font = ui->title;

    for (int pass = 0; pass < 2; pass++) { /* a long translation: the smaller font */
        x[0] = 16;
        for (int i = 0; i < HM_TABS; i++) {
            w[i] = pw_text_width(font, v->tab_names[i] ? v->tab_names[i] : "");
            x[i + 1] = x[i] + w[i] + 16;
        }
        if (x[HM_TABS] - 16 <= limit)
            break;
        font = ui->body;
    }
    from = clampi(v->tab_pos / 256, 0, HM_TABS - 1);
    to = clampi(from + 1, 0, HM_TABS - 1);
    f = v->tab_pos - from * 256;
    rrect(c, lerp(x[from], x[to], f) - 6, 10, lerp(w[from], w[to], f) + 12, 24, 12, C_TEXT,
          a * 40 / 256);
    for (int i = 0; i < HM_TABS; i++)
        if (v->tab_names[i])
            pw_text_draw(c, font, x[i], 28, v->tab_names[i], i == v->tab ? C_TEXT : C_SUB,
                         a * (i == v->tab ? 256 : 170) / 256, 0);
}

/* Where an icon is for t = its index minus the row's position (1/256): the chosen slot at t = 0,
   the small ones after it, sliding out to the left before it. */
static void slot(int t, int *x, int *w, int *h, int *alpha)
{
    int x1 = SEL_X + HM_ICON_W + SEL_GAP;

    *alpha = 256;
    if (t < 0) {
        *x = SEL_X + t * (HM_ICON_W + SEL_GAP) / 256;
        *w = HM_ICON_W;
        *h = HM_ICON_H;
        *alpha = clampi(256 + t, 0, 256);
    } else if (t < 256) {
        *x = lerp(SEL_X, x1, t);
        *w = lerp(HM_ICON_W, SMALL_W, t);
        *h = lerp(HM_ICON_H, SMALL_H, t);
    } else {
        *x = x1 + (t - 256) * (SMALL_W + SMALL_GAP) / 256;
        *w = SMALL_W;
        *h = SMALL_H;
    }
}

static void tile(pw_canvas *c, const hm_tile *g, int x, int y, int w, int h, int a)
{
    if (g->icon) {
        pw_draw_image_scaled(c, g->icon, HM_ICON_W, HM_ICON_H, x, y, w, h, a);
    } else { /* no ICON0: a dark card with a disc */
        rrect(c, x, y, w, h, 8, C_CARD, a * 230 / 256);
        pw_icon_draw(c, PW_ICON_DISC, x + w / 2, y + h / 2, h / 2, C_KNOB, a);
    }
}

static void button_pill(pw_canvas *c, const hm_ui *ui, int x, int y, const char *label, int a)
{
    int w = pw_text_width(ui->title, label) + 48;

    rrect(c, x, y, w, 30, 15, C_TEXT, a);
    pw_button_draw(c, ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, x + 18, y + 15, 16, C_INK, a);
    pw_text_draw(c, ui->title, x + 32, y + 21, label, C_INK, a, 0);
}

static void games(pw_canvas *c, const hm_ui *ui, const hm_view *v, int a)
{
    const hm_tile *sel;

    if (v->count <= 0) {
        if (v->empty)
            pw_text_draw(c, ui->title, (HM_W - pw_text_width(ui->title, v->empty)) / 2, 140,
                         v->empty, C_SUB, a, 0);
        return;
    }
    for (int i = 0; i < v->count; i++) {
        int t = i * 256 - v->selpos, x, w, h, ta;

        if (t <= -256)
            continue;
        slot(t, &x, &w, &h, &ta);
        if (x >= HM_W)
            break;
        ta = ta * a / 256;
        tile(c, &v->tiles[i], x, ROW_TOP, w, h, ta * (t > 128 ? 200 : 256) / 256);
        if (t > -128 && t < 128) /* the focus ring, fading as the row moves */
            pw_shape_rrect(c, PW_FX(x - 4), PW_FX(ROW_TOP - 4), PW_FX(w + 8), PW_FX(h + 8),
                           PW_FX(9), PW_FX(2), C_TEXT, ta * (128 - (t < 0 ? -t : t)) / 128);
    }
    sel = &v->tiles[clampi(v->sel, 0, v->count - 1)];
    pw_text_draw_fit(c, ui->big, SEL_X, TITLE_Y, sel->title ? sel->title : "", HM_W - 2 * SEL_X,
                     C_TEXT, a, 1);
    if (sel->meta)
        pw_text_draw_fit(c, ui->body, SEL_X, META_Y, sel->meta, HM_W - 2 * SEL_X, C_SUB, a, 0);
    if (v->play_label)
        button_pill(c, ui, SEL_X, PLAY_Y, v->play_label, a);
}

static void card(pw_canvas *c, const hm_ui *ui, const hm_view *v, int a)
{
    int show = v->nrows < SHOW ? v->nrows : SHOW;
    int first =
        clampi(v->row_pos - (show / 2) * 256, 0, v->nrows > show ? (v->nrows - show) * 256 : 0);

    if (v->nrows <= 0)
        return;
    rrect(c, CARD_X, CARD_Y, CARD_W, show * ROW_H + 8, 14, C_CARD, a * 215 / 256);
    for (int i = 0; i < v->nrows; i++) {
        const hm_row *r = &v->rows[i];
        int y = CARD_Y + 4 + (i * 256 - first) * ROW_H / 256;

        if (y < CARD_Y - ROW_H / 2 || y > CARD_Y + (show - 1) * ROW_H + ROW_H / 2)
            continue;
        if (i == v->row_sel)
            rrect(c, CARD_X + 6, y + 2, CARD_W - 12, ROW_H - 4, 9, C_TEXT, a * 34 / 256);
        pw_text_draw_fit(c, ui->body, CARD_X + 18, y + 18, r->label ? r->label : "",
                         r->value ? CARD_W * 3 / 5 - 18 : CARD_W - 36,
                         i == v->row_sel ? C_TEXT : PW_RGB(222, 225, 232), a, 0);
        if (r->value)
            text_right(c, ui->body, CARD_X + CARD_W - 18, y + 18, r->value, C_SUB, a);
        if (r->bar >= 0) {
            int bw = CARD_W - 36;

            pw_draw_blend(c, CARD_X + 18, y + 22, bw, 2, C_KNOB, a);
            pw_draw_blend(c, CARD_X + 18, y + 22, bw * clampi(r->bar, 0, 256) / 256, 2, C_ON, a);
        }
    }
}

static void hints(pw_canvas *c, const hm_ui *ui, const hm_view *v, int a)
{
    int x = HM_W - 14;

    if (v->hint_tabs) {
        x -= pw_text_width(ui->body, v->hint_tabs);
        pw_text_draw(c, ui->body, x, 264, v->hint_tabs, C_SUB, a, 0);
        x -= pw_text_width(ui->body, "L  R  ") + 4;
        pw_text_draw(c, ui->body, x, 264, "L  R", C_TEXT, a, 0);
        x -= 18;
    }
    if (v->hint_enter) {
        x -= pw_text_width(ui->body, v->hint_enter);
        pw_text_draw(c, ui->body, x, 264, v->hint_enter, C_SUB, a, 0);
        pw_button_draw(c, ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, x - 9, 260, 12, C_SUB, a);
    }
}

void hm_draw(pw_canvas *c, const hm_ui *ui, const hm_view *v)
{
    int a = clampi(v->appear, 0, 256);

    backdrop(c, v);
    tabs(c, ui, v, a, status(c, ui, v, a) - 12);
    if (v->tab == HM_TAB_GAMES)
        games(c, ui, v, a);
    else
        card(c, ui, v, a);
    hints(c, ui, v, a);
    if (a < 256) /* fading in, or out before a game starts */
        pw_draw_blend(c, 0, 0, HM_W, HM_H, PW_RGB(0, 0, 0), 256 - a);
}

void hm_make_background(const unsigned int *pic, int w, int h, unsigned int *out)
{
    pw_canvas c = {out, HM_W, HM_H, 512};

    pw_draw_fill(&c, 0, 0, HM_W, HM_H, PW_RGB(0, 0, 0));
    pw_draw_image_scaled(&c, pic, w, h, 0, 0, HM_W, HM_H, 256);
    pw_draw_blur(&c, 4);
    pw_draw_darken(&c, 150);
    for (int y = 100; y < HM_H; y++) /* darker towards the text */
        pw_draw_blend(&c, 0, y, HM_W, 1, PW_RGB(0, 0, 0), (y - 100) * 150 / (HM_H - 100));
}

void hm_make_gradient(unsigned int *out)
{
    pw_canvas c = {out, HM_W, HM_H, 512};

    gradient(&c);
}
