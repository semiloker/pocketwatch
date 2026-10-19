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
#define CARD_Y    44 /* the card ends above the hints (7 rows) */
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

static void rrect(hm_gfx *g, int x, int y, int w, int h, int r, unsigned int color, int alpha)
{
    g->rrect(g, x, y, w, h, r, 0, color, alpha);
}

static void text(hm_gfx *g, pw_font *f, int x, int y, const char *s, unsigned int color, int alpha)
{
    g->text(g, f, x, y, s, 0, color, alpha);
}

static void text_right(hm_gfx *g, pw_font *f, int x1, int y, const char *s, unsigned int color,
                       int alpha)
{
    g->text(g, f, x1 - pw_text_width(f, s), y, s, 0, color, alpha);
}

/* No art: a slate gradient with a faint glow of the accent at the top (into a buffer, once). */
static void gradient(pw_canvas *c)
{
    for (int y = 0; y < HM_H; y++) {
        int t = y * 256 / HM_H;

        pw_draw_fill(c, 0, y, HM_W, 1, PW_RGB(lerp(26, 8, t), lerp(29, 9, t), lerp(42, 14, t)));
    }
    pw_shape_disc(c, PW_FX(360), PW_FX(-40), PW_FX(150), C_ON, 30);
}

/* The clock and battery pill at the top right; returns its left edge. */
static int status(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    int tw = v->clock && v->clock[0] ? pw_text_width(ui->body, v->clock) : 0;
    int w = tw + (v->battery >= 0 ? 34 : 0) + 20, x = HM_W - 12 - w;

    if (w <= 20)
        return HM_W;
    rrect(g, x, 8, w, 20, 10, C_CARD, a * 190 / 256);
    if (tw)
        text(g, ui->body, x + 10, 22, v->clock, C_TEXT, a);
    if (v->battery >= 0) {
        int bx = x + w - 30, fill = 14 * clampi(v->battery, 0, 100) / 100;

        g->rrect(g, bx, 13, 18, 10, 2, PW_FX(1) + 4, C_TEXT, a);
        g->fill(g, bx + 18, 16, 2, 4, C_TEXT, a);
        g->fill(g, bx + 2, 15, fill, 6, v->charging ? PW_RGB(120, 220, 120) : C_TEXT, a);
    }
    return x;
}

/* The section names at the top left; a pill glides under the chosen one. */
static void tabs(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a, int limit)
{
    int x[HM_TABS + 1], w[HM_TABS], from, to, f;
    pw_font *font = ui->title;

    for (int pass = 0; pass < 3; pass++) { /* too wide: closer together, then the smaller font */
        int gap = pass == 1 ? 10 : 16;

        font = pass == 2 ? ui->body : ui->title;
        x[0] = 16;
        for (int i = 0; i < HM_TABS; i++) {
            w[i] = pw_text_width(font, v->tab_names[i] ? v->tab_names[i] : "");
            x[i + 1] = x[i] + w[i] + gap;
        }
        if (x[HM_TABS] - gap <= limit)
            break;
    }
    from = clampi(v->tab_pos / 256, 0, HM_TABS - 1);
    to = clampi(from + 1, 0, HM_TABS - 1);
    f = v->tab_pos - from * 256;
    /* Still too wide: the strip slides so the chosen one stays in view; names past the edges
       are left out. */
    if (x[HM_TABS] > limit) {
        int want = lerp(x[from] + w[from], x[to] + w[to], f) + 8 - limit,
            shift = want > 0 ? want : 0;

        for (int i = 0; i <= HM_TABS; i++)
            x[i] -= shift;
    }
    rrect(g, lerp(x[from], x[to], f) - 6, 10, lerp(w[from], w[to], f) + 12, 24, 12, C_TEXT,
          a * 40 / 256);
    for (int i = 0; i < HM_TABS; i++)
        if (v->tab_names[i] && x[i] >= 4 && x[i] + w[i] <= limit + 4)
            text(g, font, x[i], 28, v->tab_names[i], i == v->tab ? C_TEXT : C_SUB,
                 a * (i == v->tab ? 256 : 170) / 256);
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

static void tile(hm_gfx *g, const hm_tile *t, int x, int y, int w, int h, int a)
{
    if (t->icon) {
        g->image(g, t->icon, HM_ICON_W, HM_ICON_H, HM_ICON_W, x, y, w, h, a);
    } else { /* no ICON0: a dark card with a disc */
        rrect(g, x, y, w, h, 8, C_CARD, a * 230 / 256);
        g->icon(g, PW_ICON_DISC, x + w / 2, y + h / 2, h / 2, C_KNOB, a);
    }
}

static void button_pill(hm_gfx *g, const hm_ui *ui, int x, int y, const char *label, int a)
{
    int w = pw_text_width(ui->title, label) + 48;

    rrect(g, x, y, w, 30, 15, C_TEXT, a);
    g->button(g, ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, x + 18, y + 15, 16, C_INK, a);
    text(g, ui->title, x + 32, y + 21, label, C_INK, a);
}

static void games(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    const hm_tile *sel;

    if (v->count <= 0) {
        if (v->empty)
            text(g, ui->title, (HM_W - pw_text_width(ui->title, v->empty)) / 2, 140, v->empty,
                 C_SUB, a);
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
        tile(g, &v->tiles[i], x, ROW_TOP, w, h, ta * (t > 128 ? 200 : 256) / 256);
        if (t > -128 && t < 128) /* the focus ring, fading as the row moves */
            g->rrect(g, x - 4, ROW_TOP - 4, w + 8, h + 8, 9, PW_FX(2), C_TEXT,
                     ta * (128 - (t < 0 ? -t : t)) / 128);
    }
    sel = &v->tiles[clampi(v->sel, 0, v->count - 1)];
    g->text(g, ui->big, SEL_X, TITLE_Y, sel->title ? sel->title : "", HM_W - 2 * SEL_X, C_TEXT, a);
    if (sel->meta)
        g->text(g, ui->body, SEL_X, META_Y, sel->meta, HM_W - 2 * SEL_X, C_SUB, a);
    if (v->play_label)
        button_pill(g, ui, SEL_X, PLAY_Y, v->play_label, a);
}

/* A small arrow (dir 1 right, -1 left) centred at x, y: two strokes of a rounded line. */
static void chevron(hm_gfx *g, int x, int y, int dir, int a)
{
    for (int k = 0; k < 4; k++) {
        int dx = dir > 0 ? k - 2 : 2 - k;

        g->fill(g, x + dx, y - 4 + k, 2, 1, C_SUB, a);
        g->fill(g, x + dx, y + 4 - k, 2, 1, C_SUB, a);
    }
    g->fill(g, x + (dir > 0 ? 2 : -2), y, 2, 1, C_SUB, a);
}

static void card(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    int show = v->nrows < SHOW ? v->nrows : SHOW;
    int first =
        clampi(v->row_pos - (show / 2) * 256, 0, v->nrows > show ? (v->nrows - show) * 256 : 0);

    if (v->nrows <= 0)
        return;
    rrect(g, CARD_X, CARD_Y, CARD_W, show * ROW_H + 8, 14, C_CARD, a * 215 / 256);
    for (int i = 0; i < v->nrows; i++) {
        const hm_row *r = &v->rows[i];
        int y = CARD_Y + 4 + (i * 256 - first) * ROW_H / 256;

        if (y < CARD_Y - ROW_H / 2 || y > CARD_Y + (show - 1) * ROW_H + ROW_H / 2)
            continue;
        if (r->kind == HM_ROW_HEADER) { /* a group's name, in the accent colour */
            g->text(g, ui->body, CARD_X + 18, y + 20, r->label ? r->label : "", CARD_W - 36, C_ON,
                    a);
            g->fill(g, CARD_X + 18, y + ROW_H - 3, CARD_W - 36, 1, C_KNOB, a);
            continue;
        }
        if (i == v->row_sel)
            rrect(g, CARD_X + 6, y + 2, CARD_W - 12, ROW_H - 4, 9, C_TEXT, a * 34 / 256);
        g->text(g, ui->body, CARD_X + 18, y + 18, r->label ? r->label : "",
                r->value ? CARD_W * 3 / 5 - 18 : CARD_W - 36,
                i == v->row_sel ? C_TEXT : PW_RGB(222, 225, 232), a);
        if (r->value && r->kind == HM_ROW_CHOICE && i == v->row_sel) {
            /* the value between the ways to change it */
            int x1 = CARD_X + CARD_W - 18, tw = pw_text_width(ui->body, r->value);

            text_right(g, ui->body, x1 - 14, y + 18, r->value, C_TEXT, a);
            chevron(g, x1 - 6, y + 14, 1, a);
            chevron(g, x1 - 22 - tw, y + 14, -1, a);
        } else if (r->value) {
            text_right(g, ui->body, CARD_X + CARD_W - 18, y + 18, r->value, C_SUB, a);
        } else if (r->kind == HM_ROW_ACTION) {
            chevron(g, CARD_X + CARD_W - 22, y + 14, 1, a);
        }
        if (r->bar >= 0) {
            int bw = CARD_W - 36;

            g->fill(g, CARD_X + 18, y + 22, bw, 2, C_KNOB, a);
            g->fill(g, CARD_X + 18, y + 22, bw * clampi(r->bar, 0, 256) / 256, 2, C_ON, a);
        }
    }
}

static void hints(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    int x = HM_W - 14;

    if (v->hint_tabs) {
        x -= pw_text_width(ui->body, v->hint_tabs);
        text(g, ui->body, x, 264, v->hint_tabs, C_SUB, a);
        x -= pw_text_width(ui->body, "L  R  ") + 4;
        text(g, ui->body, x, 264, "L  R", C_TEXT, a);
        x -= 18;
    }
    if (v->hint_enter) {
        x -= pw_text_width(ui->body, v->hint_enter);
        text(g, ui->body, x, 264, v->hint_enter, C_SUB, a);
        g->button(g, ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, x - 9, 260, 12, C_SUB, a);
        x -= 26;
    }
    if (v->hint_options) {
        x -= pw_text_width(ui->body, v->hint_options);
        text(g, ui->body, x, 264, v->hint_options, C_SUB, a);
        g->button(g, PW_BTN_TRIANGLE, x - 9, 260, 12, C_SUB, a);
    }
}

/* The HOME panel: the screen dims, a card rises a little as it fades in. */
static void panel(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int pics = 0, wide = 0;

    for (int i = 0; i < v->panel_count; i++) {
        pics |= v->panel_icons[i] != 0;
        wide |= v->panel_values[i] != 0;
    }
    {
        int p = clampi(v->panel, 0, 256), n = v->panel_count, rh = pics ? 44 : ROW_H,
            w = pics || wide ? 400 : 260, hint = v->panel_hint ? 22 : 0, h = 40 + n * rh + 8 + hint,
            y0 = (HM_H - h) / 2 + (256 - p) * 12 / 256, x0 = (HM_W - w) / 2,
            tx = x0 + (pics ? 96 : 20);

        g->fill(g, 0, 0, HM_W, HM_H, PW_RGB(0, 0, 0), p * 150 / 256);
        rrect(g, x0, y0, w, h, 16, C_CARD, p * 240 / 256);
        if (v->panel_title)
            g->text(g, ui->title, x0 + 20, y0 + 26, v->panel_title, w - 40, C_SUB, p);
        g->rrect(g, x0 + 8, y0 + 38 + v->panel_pos * rh / 256, w - 16, rh - 4, 9, 0, C_TEXT,
                 p * 40 / 256);
        for (int i = 0; i < n; i++) {
            int y = y0 + 38 + i * rh, base = y + rh / 2 + 4;

            if (v->panel_icons[i])
                g->image(g, v->panel_icons[i], HM_ICON_W, HM_ICON_H, HM_ICON_W, x0 + 18, y + 4, 64,
                         36, p);
            if (v->panel_items[i])
                g->text(g, ui->body, tx, base, v->panel_items[i], v->panel_values[i] ? 150 : w - 40,
                        i == v->panel_sel ? C_TEXT : PW_RGB(222, 225, 232), p);
            if (v->panel_values[i])
                text_right(g, ui->body, x0 + w - 20, base, v->panel_values[i], C_SUB, p);
        }
        if (v->panel_hint) {
            int tw = pw_text_width(ui->body, v->panel_hint);

            text(g, ui->body, x0 + w - 20 - tw, y0 + h - 12, v->panel_hint, C_SUB, p);
            g->button(g, ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, x0 + w - 32 - tw,
                      y0 + h - 16, 12, C_SUB, p);
        }
    }
}

/* A message card: the screen dims, the card holds a title, a line and the way out. */
static void sheet(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int p = clampi(v->sheet, 0, 256), w = 400, h = 112, x0 = (HM_W - w) / 2,
        y0 = (HM_H - h) / 2 + (256 - p) * 12 / 256;

    g->fill(g, 0, 0, HM_W, HM_H, PW_RGB(0, 0, 0), p * 170 / 256);
    rrect(g, x0, y0, w, h, 16, C_CARD, p * 245 / 256);
    g->icon(g, PW_ICON_CARD, x0 + 30, y0 + 30, 14, C_TEXT, p);
    if (v->sheet_title)
        text(g, ui->title, x0 + 52, y0 + 36, v->sheet_title, C_TEXT, p);
    if (v->sheet_text)
        g->text(g, ui->body, x0 + 20, y0 + 68, v->sheet_text, w - 40, C_SUB, p);
    if (v->sheet_busy)
        for (int i = 0; i < 3; i++) { /* three dots, a wave through them */
            int t = (int)((v->t_ms / 4 + (unsigned int)(i * 60)) % 256),
                k = t < 128 ? t * 2 : (255 - t) * 2;

            g->rrect(g, x0 + w - 56 + i * 12, y0 + 28, 6, 6, 3, 0, C_ON,
                     p * (90 + k * 166 / 256) / 256);
        }
    if (v->sheet_hint) {
        int tw = pw_text_width(ui->body, v->sheet_hint), x = x0 + w - 20 - tw;

        text(g, ui->body, x, y0 + h - 14, v->sheet_hint, C_SUB, p);
        g->button(g, ui->cross_enter ? PW_BTN_CIRCLE : PW_BTN_CROSS, x - 12, y0 + h - 18, 12, C_SUB,
                  p);
        if (v->sheet_again) { /* Enter, to the left of the way out */
            x -= 30 + pw_text_width(ui->body, v->sheet_again);
            text(g, ui->body, x, y0 + h - 14, v->sheet_again, C_TEXT, p);
            g->button(g, ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, x - 12, y0 + h - 18, 12,
                      C_TEXT, p);
        }
    }
}

void hm_prefetch(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    /* the games next to the chosen one, nearest first, as games() draws them */
    for (int d = 1; d <= 3; d++)
        for (int s = -1; s <= 1; s += 2) {
            int i = v->sel + s * d;
            const hm_tile *t;

            if (i < 0 || i >= v->count)
                continue;
            t = &v->tiles[i];
            if (!g->warm(g, ui->big, t->title ? t->title : "", HM_W - 2 * SEL_X) ||
                (t->meta && !g->warm(g, ui->body, t->meta, HM_W - 2 * SEL_X)))
                return; /* out of time this frame */
        }
    /* the sections' rows, as card() draws them: around the cursor in this one, the first ones of
       the others (a long list - the music - is not made all at once) */
    for (int k = 0; k < HM_TABS; k++) {
        int from = k == v->tab ? v->row_sel - SHOW : 0,
            to = k == v->tab ? v->row_sel + SHOW : SHOW + 1;

        for (int i = from < 0 ? 0 : from; i < v->other_nrows[k] && i <= to && v->other_rows[k];
             i++) {
            const hm_row *r = &v->other_rows[k][i];

            if (r->kind == HM_ROW_HEADER
                    ? !g->warm(g, ui->body, r->label ? r->label : "", CARD_W - 36)
                    : !g->warm(g, ui->body, r->label ? r->label : "",
                               r->value ? CARD_W * 3 / 5 - 18 : CARD_W - 36) ||
                          (r->value && !g->warm(g, ui->body, r->value, 0)))
                return;
        }
    }
    for (int i = 0; i < v->panel_count; i++)
        if (v->panel_items[i] && !g->warm(g, ui->body, v->panel_items[i], 0))
            return;
}

void hm_draw(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int a = clampi(v->appear, 0, 256);

    g->backdrop(g, v->bg, v->bg_prev && v->bg_mix < 256 ? v->bg_prev : 0, v->bg_mix);
    if (v->viewer >= 256) { /* the photo alone, and its name for a moment */
        if (v->viewer_caption && v->caption > 0) {
            int w = pw_text_width(ui->body, v->viewer_caption) + 28;

            w = w > HM_W / 2 - 12 ? HM_W / 2 - 12 : w;
            rrect(g, 12, HM_H - 36, w, 24, 12, C_CARD, v->caption * 200 / 256);
            g->text(g, ui->body, 26, HM_H - 20, v->viewer_caption, w - 28, C_TEXT, v->caption);
        }
        if (v->caption > 0 && (v->viewer_wallpaper || v->viewer_slideshow)) {
            /* what the buttons do, bottom right, on a pill of their own */
            int ww = v->viewer_wallpaper ? pw_text_width(ui->body, v->viewer_wallpaper) : 0,
                sw = v->viewer_slideshow ? pw_text_width(ui->body, v->viewer_slideshow) : 0,
                w = 22 + (sw ? 15 + sw : 0) + (ww ? 15 + ww : 0) + (sw && ww ? 12 : 0),
                x = HM_W - 12 - w + 10; /* a group: the glyph (centred 6 in), its text 15 in */

            rrect(g, HM_W - 12 - w, HM_H - 36, w, 24, 12, C_CARD, v->caption * 200 / 256);
            if (sw) {
                g->button(g, PW_BTN_TRIANGLE, x + 6, HM_H - 24, 12, C_TEXT, v->caption);
                text(g, ui->body, x + 15, HM_H - 20, v->viewer_slideshow, C_TEXT, v->caption);
                x += 15 + sw + 12;
            }
            if (ww) {
                g->button(g, PW_BTN_SQUARE, x + 6, HM_H - 24, 12, C_TEXT, v->caption);
                text(g, ui->body, x + 15, HM_H - 20, v->viewer_wallpaper, C_TEXT, v->caption);
            }
        }
        return;
    }
    tabs(g, ui, v, a, status(g, ui, v, a) - 12);
    if (v->tab == HM_TAB_GAMES || v->tab == HM_TAB_PHOTOS) /* the same row of pictures */
        games(g, ui, v, a);
    else
        card(g, ui, v, a);
    hints(g, ui, v, a);
    if (v->panel > 0)
        panel(g, ui, v);
    if (v->sheet > 0)
        sheet(g, ui, v);
    if (a < 256) /* fading in, or out before a game starts */
        g->fill(g, 0, 0, HM_W, HM_H, PW_RGB(0, 0, 0), 256 - a);
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

void hm_make_photo(const unsigned int *pic, int w, int h, unsigned int *out)
{
    pw_canvas c = {out, HM_W, HM_H, 512};
    int fw = HM_W, fh = h * HM_W / (w > 0 ? w : 1);

    if (fh > HM_H) {
        fh = HM_H;
        fw = w * HM_H / (h > 0 ? h : 1);
    }
    pw_draw_fill(&c, 0, 0, HM_W, HM_H, PW_RGB(0, 0, 0));
    pw_draw_image_scaled(&c, pic, w, h, (HM_W - fw) / 2, (HM_H - fh) / 2, fw, fh, 256);
}

void hm_make_gradient(unsigned int *out)
{
    pw_canvas c = {out, HM_W, HM_H, 512};

    gradient(&c);
}
