// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Modern (docs/ui/styles.md): the sections as pills at the top left - an icon each, the chosen
 * one opening into a white pill with its name; Games as a row of icons over the chosen game's
 * blurred art; Photos as a grid; Music as a card of what plays beside the tracks; the other
 * sections as lists whose chosen row is a white pill gliding under the text.
 */
#include <string.h>

#include "view_internal.h"

#define ROW_TOP   56 /* Games: the icons' top edge */
#define SEL_X     24 /* the chosen icon's left edge */
#define SEL_GAP   14 /* after the chosen icon */
#define SMALL_W   96 /* the others */
#define SMALL_H   53
#define SMALL_GAP 10
#define TITLE_Y   176
#define META_Y    196
#define PLAY_Y    210

#define LIST_X0  24 /* the lists */
#define LIST_X1  456
#define LIST_Y0  44
#define LIST_Y1  248
#define PITCH    30 /* a row; 38 with pictures (Play Time) */
#define PITCH_PIC 38

#define PG_X     26 /* Photos: the grid */
#define PG_Y     62
#define PG_W     100
#define PG_H     56
#define PG_GAP   9
#define PG_PITCH 66

#define MU_X 24 /* Music: what plays, on a card */
#define MU_W 168
#define MU_ART 120

#define C_HEAD PW_RGB(110, 176, 255) /* a group's name */

typedef struct {
    int x0, x1, y0, y1, pitch;
    int stack; /* the value under the label (a narrow list: the tracks) */
} box;

static unsigned int mix(unsigned int c0, unsigned int c1, int t) /* t 0-256 */
{
    unsigned int out = 0xFF000000u;

    for (int s = 0; s < 24; s += 8)
        out |= (unsigned int)hm_lerp((int)(c0 >> s & 0xFF), (int)(c1 >> s & 0xFF), t) << s;
    return out;
}

/* ---- the top: section pills, the clock and battery ---- */

static int status(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    int tw = v->clock && v->clock[0] ? pw_text_width(ui->body, v->clock) : 0;
    int w = tw + (v->battery >= 0 ? 34 : 0) + 20, x = HM_W - 12 - w;

    if (w <= 20)
        return HM_W;
    g->rrect(g, x, 9, w, 24, 12, 0, C_CARD, a * 190 / 256);
    if (tw)
        g->text(g, ui->body, x + 10, 25, v->clock, 0, C_TEXT, a);
    if (v->battery >= 0) {
        int bx = x + w - 30, fill = 14 * hm_clampi(v->battery, 0, 100) / 100;

        g->rrect(g, bx, 16, 18, 10, 2, PW_FX(1) + 4, C_TEXT, a);
        g->fill(g, bx + 18, 19, 2, 4, C_TEXT, a);
        g->fill(g, bx + 2, 18, fill, 6, v->charging ? PW_RGB(120, 220, 120) : C_TEXT, a);
    }
    return x;
}

/* Each section an icon; the chosen one (following tab_pos, so it glides) opens into a white
   pill that carries its name. Too little room: the names in the smaller font. */
static void pills(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a, int limit)
{
    pw_font *f = ui->title;
    int widest = 0, x = 12;

    for (int i = 0; i < HM_TABS; i++)
        if (v->tab_names[i] && pw_text_width(ui->title, v->tab_names[i]) > widest)
            widest = pw_text_width(ui->title, v->tab_names[i]);
    if (12 + HM_TABS * 32 + widest + 12 > limit)
        f = ui->body;
    for (int i = 0; i < HM_TABS; i++) {
        int open = hm_near(i * 256 - v->tab_pos),
            nw = v->tab_names[i] ? pw_text_width(f, v->tab_names[i]) : 0,
            w = 28 + (nw + 10) * open / 256;

        if (open > 0)
            g->rrect(g, x, 9, w, 24, 12, 0, C_TEXT, a * open / 256 * 235 / 256);
        else
            g->rrect(g, x, 9, w, 24, 12, 0, C_CARD, a * 120 / 256);
        g->icon(g, hm_icon_of_tab(i), x + 14, 21, 16, mix(C_TEXT, C_INK, open),
                a * (190 + 66 * open / 256) / 256);
        if (open > 64 && v->tab_names[i])
            g->text(g, f, x + 27, f == ui->title ? 26 : 25, v->tab_names[i], 0, C_INK,
                    a * (open - 64) / 192);
        x += w + 4;
    }
}

/* ---- Games: the row of icons, the chosen game's title, its line and Play ---- */

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
        *alpha = hm_clampi(256 + t, 0, 256);
    } else if (t < 256) {
        *x = hm_lerp(SEL_X, x1, t);
        *w = hm_lerp(HM_ICON_W, SMALL_W, t);
        *h = hm_lerp(HM_ICON_H, SMALL_H, t);
    } else {
        *x = x1 + (t - 256) * (SMALL_W + SMALL_GAP) / 256;
        *w = SMALL_W;
        *h = SMALL_H;
    }
}

static void button_pill(hm_gfx *g, const hm_ui *ui, int x, int y, const char *label, int a)
{
    int w = pw_text_width(ui->title, label) + 48;

    g->rrect(g, x, y, w, 30, 15, 0, C_TEXT, a);
    g->button(g, ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, x + 18, y + 15, 16, C_INK, a);
    g->text(g, ui->title, x + 32, y + 21, label, 0, C_INK, a);
}

static void games(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    const hm_tile *sel;

    if (v->count <= 0) {
        if (v->empty)
            hm_text_center(g, ui->title, HM_W / 2, 140, v->empty, HM_W - 40, C_SUB, a);
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
        hm_tile_draw(g, v->tiles[i].icon, x, ROW_TOP, w, h, ta * (t > 128 ? 200 : 256) / 256);
        if (t > -128 && t < 128) /* the focus ring, fading as the row moves */
            g->rrect(g, x - 4, ROW_TOP - 4, w + 8, h + 8, 9, PW_FX(2), C_TEXT,
                     ta * (128 - (t < 0 ? -t : t)) / 128);
    }
    sel = &v->tiles[hm_clampi(v->sel, 0, v->count - 1)];
    g->text(g, ui->big, SEL_X, TITLE_Y, sel->title ? sel->title : "", HM_W - 2 * SEL_X, C_TEXT, a);
    if (sel->meta)
        g->text(g, ui->body, SEL_X, META_Y, sel->meta, HM_W - 2 * SEL_X, C_SUB, a);
    if (v->play_label)
        button_pill(g, ui, SEL_X, PLAY_Y, v->play_label, a);
}

/* ---- Photos: a grid, the chosen one framed, its name above ---- */

static int photo_first(const hm_view *v) /* the top row on screen (1/256 rows): the chosen row
                                             second */
{
    int rows = (v->count + HM_PHOTO_COLS - 1) / HM_PHOTO_COLS;

    return hm_clampi(v->selpos / HM_PHOTO_COLS - 256, 0, rows > 3 ? (rows - 3) * 256 : 0);
}

static void photos(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    int sel = hm_clampi(v->sel, 0, v->count - 1), first;
    const hm_tile *t;

    if (v->count <= 0) {
        if (v->empty)
            hm_text_center(g, ui->title, HM_W / 2, 140, v->empty, HM_W - 40, C_SUB, a);
        return;
    }
    t = &v->tiles[sel];
    first = photo_first(v);
    {
        int mw = t->meta ? pw_text_width(ui->body, t->meta) : 0;

        g->text(g, ui->title, PG_X, 52, t->title ? t->title : "", LIST_X1 - PG_X - mw - 16, C_TEXT,
                a);
        hm_text_right(g, ui->body, LIST_X1, 52, t->meta, C_SUB, a);
    }
    for (int i = 0; i < v->count; i++) {
        int r = i / HM_PHOTO_COLS, col = i % HM_PHOTO_COLS,
            y = PG_Y + (r * 256 - first) * PG_PITCH / 256, x = col * (PG_W + PG_GAP) + PG_X, ta;

        if (y + PG_H <= 40 || y >= 254)
            continue;
        /* rows leaving the grid fade out under the name, above the hints */
        ta = a * hm_clampi(y < PG_Y ? 256 - (PG_Y - y) * 256 / 24 : 256, 0, 256) / 256;
        ta = ta * hm_clampi(y + PG_H > 250 ? 256 - (y + PG_H - 250) * 256 / 24 : 256, 0, 256) / 256;
        hm_tile_draw(g, v->tiles[i].icon, x, y, PG_W, PG_H, ta * (i == sel ? 256 : 200) / 256);
        if (i == sel)
            g->rrect(g, x - 3, y - 3, PG_W + 6, PG_H + 6, 8, PW_FX(2), C_TEXT, ta);
    }
}

/* ---- the lists ---- */

static int has_pictures(const hm_row *rows, int n)
{
    for (int i = 0; i < n; i++)
        if (rows[i].icon)
            return 1;
    return 0;
}

static box box_of(int tab, const hm_row *rows, int n)
{
    box b = {LIST_X0, LIST_X1, LIST_Y0, LIST_Y1, PITCH, 0};

    if (tab == HM_TAB_MUSIC) { /* beside the card of what plays: the artist under the title */
        b.x0 = MU_X + MU_W + 12;
        b.y1 = LIST_Y1 - 6; /* (its last row fades out above the hints) */
        b.pitch = PITCH_PIC;
        b.stack = 1;
    }
    if (has_pictures(rows, n))
        b.pitch = PITCH_PIC;
    return b;
}

/* Where a row's label starts and how wide it may be: up to its value (with the room for the
   arrows of a choice, chosen or not, so moving makes no new text). The drawing and the prefetch
   agree. */
static void label_room(const hm_ui *ui, const hm_row *r, box b, int playing, int *lx, int *lw)
{
    int keep = r->toggle ? 30 + 12
               : r->value && !b.stack
                   ? pw_text_width(ui->body, r->value) + (r->kind == HM_ROW_CHOICE ? 30 : 0) + 14
                   : 16;

    *lx = b.x0 + 14;
    if (r->icon)
        *lx += (b.pitch - 10) * 9 / 5 + 10;
    if (playing)
        *lx += 14;
    *lw = r->kind == HM_ROW_HEADER || r->kind == HM_ROW_NOTE ? b.x1 - b.x0 - 28
                                                            : b.x1 - 14 - keep - *lx;
    if (*lw < 40)
        *lw = 40;
}

/* Three bars that bounce: the track playing. */
static void bars(hm_gfx *g, int x, int y, unsigned int t_ms, int moving, unsigned int color, int a)
{
    for (int i = 0; i < 3; i++) {
        int k = (int)((t_ms / 3 + (unsigned int)i * 90) % 256),
            h = moving ? 3 + (k < 128 ? k : 255 - k) * 7 / 128 : 4;

        g->fill(g, x + i * 4, y - h, 3, h, color, a);
    }
}

static void list(hm_gfx *g, const hm_ui *ui, const hm_row *rows, int n, int sel, int pos, box b,
                 int a, unsigned int t_ms, int playing, int moving)
{
    int show = (b.y1 - b.y0) / b.pitch,
        first = hm_clampi(pos - (show / 2) * 256, 0, n > show ? (n - show) * 256 : 0);

    if (sel >= 0 && sel < n && rows[sel].kind != HM_ROW_HEADER && rows[sel].kind != HM_ROW_NOTE) {
        int py = b.y0 + (pos - first) * b.pitch / 256;

        g->rrect(g, b.x0, py + 2, b.x1 - b.x0, b.pitch - 4, (b.pitch - 4) / 2, 0, C_TEXT,
                 a * 235 / 256);
    }
    for (int i = 0; i < n; i++) {
        const hm_row *r = &rows[i];
        int y = b.y0 + (i * 256 - first) * b.pitch / 256, base = y + b.pitch / 2 + 5, mid,
            on = i == sel && hm_near(i * 256 - pos) > 128, lx, lw, ra;
        unsigned int ct = on ? C_INK : C_DIM, cv = on ? C_INK2 : C_SUB;

        if (y <= b.y0 - b.pitch || y >= b.y1)
            continue;
        /* rows leaving the list fade out at its edges */
        ra = a * hm_clampi(y < b.y0 ? 256 - (b.y0 - y) * 256 / b.pitch : 256, 0, 256) / 256;
        ra = ra * hm_clampi(y + b.pitch > b.y1 ? 256 - (y + b.pitch - b.y1) * 256 / b.pitch : 256,
                            0, 256) /
             256;
        mid = y + b.pitch / 2;
        label_room(ui, r, b, playing >= 0 && i == playing, &lx, &lw);
        if (r->kind == HM_ROW_HEADER) {
            g->text(g, ui->body, lx, y + b.pitch - 8, r->label ? r->label : "", lw, C_HEAD, ra);
            continue;
        }
        if (r->kind == HM_ROW_NOTE) {
            g->text(g, ui->body, lx, base, r->label ? r->label : "", lw, C_SUB, ra * 200 / 256);
            continue;
        }
        if (r->icon) {
            int ih = b.pitch - 10, iw = ih * 9 / 5;

            hm_tile_draw(g, r->icon, b.x0 + 10, y + 5, iw, ih, ra);
        }
        if (i == playing)
            bars(g, lx - 14, b.stack ? y + 17 : base, t_ms, moving, on ? C_ON : C_HEAD, ra);
        if (b.stack) { /* the label, the value under it */
            g->text(g, ui->title, lx, y + 17, r->label ? r->label : "", lw,
                    i == playing && !on ? C_HEAD : ct, ra);
            if (r->value)
                g->text(g, ui->body, lx, y + 31, r->value, lw, cv, ra);
            if (r->bar >= 0) {
                int bw = b.x1 - 14 - lx;

                g->fill(g, lx, y + b.pitch - 5, bw, 2, on ? PW_RGB(200, 204, 214) : C_KNOB, ra);
                g->fill(g, lx, y + b.pitch - 5, bw * hm_clampi(r->bar, 0, 256) / 256, 2, C_ON, ra);
            }
            continue;
        }
        g->text(g, ui->title, lx, base, r->label ? r->label : "", lw,
                i == playing && !on ? C_HEAD : ct, ra);
        if (r->toggle) {
            hm_switch(g, b.x1 - 14 - 30, mid, r->toggle == 2, ra);
        } else if (r->value && r->kind == HM_ROW_CHOICE && on) {
            /* the value between the ways to change it */
            int x1 = b.x1 - 16, tw = pw_text_width(ui->body, r->value);

            hm_text_right(g, ui->body, x1 - 14, base - 1, r->value, cv, ra);
            hm_chevron(g, x1 - 6, mid, 1, C_INK, ra);
            hm_chevron(g, x1 - 22 - tw, mid, -1, C_INK, ra);
        } else if (r->value) {
            hm_text_right(g, ui->body, b.x1 - 14, base - 1, r->value, cv, ra);
        } else if (r->kind == HM_ROW_ACTION) {
            hm_chevron(g, b.x1 - 18, mid, 1, on ? C_INK : C_SUB, ra);
        }
        if (r->bar >= 0) { /* under the label: how much of the most (Play Time), of the track */
            int bw = b.x1 - 14 - lx;

            g->fill(g, lx, y + b.pitch - 8, bw, 2, on ? PW_RGB(200, 204, 214) : C_KNOB, ra);
            g->fill(g, lx, y + b.pitch - 8, bw * hm_clampi(r->bar, 0, 256) / 256, 2, C_ON, ra);
        }
    }
    if (n > show) { /* where in the list: a thin bar at the right */
        int h = b.y1 - b.y0, th = hm_clampi(h * show / n, 12, h),
            ty = b.y0 + (h - th) * first / ((n - show) * 256);

        g->rrect(g, b.x1 + 8, b.y0, 3, h, 1, 0, C_TEXT, a * 40 / 256);
        g->rrect(g, b.x1 + 8, ty, 3, th, 1, 0, C_TEXT, a * 170 / 256);
    }
}

/* ---- Music: what plays on a card at the left, the tracks at the right ---- */

static void music(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    static const unsigned int k_tint[6] = {PW_RGB(64, 92, 200),  PW_RGB(150, 60, 140),
                                           PW_RGB(36, 132, 122), PW_RGB(196, 104, 48),
                                           PW_RGB(92, 70, 180),  PW_RGB(48, 118, 186)};
    /* (no tracks at all: the card stays empty, the message where the tracks would be) */
    const char *title = v->mu_title ? v->mu_title : v->nrows > 1 ? v->rows[0].label : 0;
    unsigned int h = 2166136261u;
    int x = MU_X, y = LIST_Y0, ax = x + (MU_W - MU_ART) / 2, ay = y + 12, ring;
    box b = box_of(HM_TAB_MUSIC, v->rows + 1, v->nrows - 1);

    for (const char *s = title ? title : ""; *s; s++) /* the art's colour, by the title */
        h = (h ^ (unsigned char)*s) * 16777619u;
    g->rrect(g, x, y, MU_W, LIST_Y1 - y, 16, 0, C_CARD, a * 200 / 256);
    ring = v->row_sel == 0 ? hm_near(v->row_pos) : 0;
    if (ring > 0)
        g->rrect(g, x - 3, y - 3, MU_W + 6, LIST_Y1 - y + 6, 18, PW_FX(2), C_TEXT,
                 a * ring / 256);
    g->rrect(g, ax, ay, MU_ART, MU_ART, 14, 0, k_tint[h % 6], a);
    g->rrect(g, ax, ay, MU_ART, MU_ART / 2, 14, 0, C_TEXT, a * 18 / 256); /* light from above */
    g->icon(g, HM_ICON_MUSIC, ax + MU_ART / 2, ay + MU_ART / 2, 56, C_TEXT, a * 225 / 256);
    g->rrect(g, ax + MU_ART - 36, ay + MU_ART - 36, 28, 28, 14, 0, C_TEXT, a);
    g->icon(g, v->mu_state == 1 ? HM_ICON_PAUSE : HM_ICON_PLAY, ax + MU_ART - 22 + (v->mu_state == 1 ? 0 : 1),
            ay + MU_ART - 22, 12, C_INK, a);
    if (title) /* the smaller font when the title is long */
        g->text(g, pw_text_width(ui->title, title) <= MU_W - 28 ? ui->title : ui->body, x + 14,
                ay + MU_ART + 24, title, MU_W - 28, C_TEXT, a);
    if (v->mu_artist && v->mu_artist[0])
        g->text(g, ui->body, x + 14, ay + MU_ART + 40, v->mu_artist, MU_W - 28, C_SUB, a);
    if (v->mu_bar >= 0) {
        int bw = MU_W - 28;

        g->rrect(g, x + 14, ay + MU_ART + 48, bw, 3, 1, 0, C_KNOB, a);
        g->rrect(g, x + 14, ay + MU_ART + 48, bw * hm_clampi(v->mu_bar, 0, 256) / 256, 3, 1, 0, C_ON,
                 a);
    }
    if (v->mu_time)
        g->text(g, ui->body, x + 14, ay + MU_ART + 64, v->mu_time, MU_W - 28, C_SUB, a);
    if (v->nrows > 1)
        list(g, ui, v->rows + 1, v->nrows - 1, v->row_sel - 1, v->row_pos - 256, b, a, v->t_ms,
             v->mu_cur, v->mu_state == 1);
    else if (v->nrows == 1)
        hm_text_center(g, ui->title, (b.x0 + b.x1) / 2, (LIST_Y0 + LIST_Y1) / 2, v->rows[0].label,
                       b.x1 - b.x0 - 20, C_SUB, a);
}

/* ---- over everything: the HOME panel, a message card ---- */

static void panel(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int pics = 0, wide = 0;

    for (int i = 0; i < v->panel_count; i++) {
        pics |= v->panel_icons[i] != 0;
        wide |= v->panel_values[i] != 0;
    }
    {
        int p = hm_clampi(v->panel, 0, 256), n = v->panel_count, rh = pics ? 44 : PITCH,
            w = pics || wide ? 400 : 260, hint = v->panel_hint ? 22 : 0, h = 40 + n * rh + 8 + hint,
            y0 = (HM_H - h) / 2 + (256 - p) * 12 / 256, x0 = (HM_W - w) / 2,
            tx = x0 + (pics ? 96 : 22);

        g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, p * 150 / 256);
        g->rrect(g, x0, y0, w, h, 16, 0, C_CARD, p * 240 / 256);
        if (v->panel_title)
            g->text(g, ui->title, x0 + 20, y0 + 26, v->panel_title, w - 40, C_SUB, p);
        g->rrect(g, x0 + 8, y0 + 38 + v->panel_pos * rh / 256, w - 16, rh - 4, (rh - 4) / 2 < 12 ? (rh - 4) / 2 : 12,
                 0, C_TEXT, p * 235 / 256);
        for (int i = 0; i < n; i++) {
            int y = y0 + 38 + i * rh, base = y + rh / 2 + 4,
                on = hm_near(i * 256 - v->panel_pos) > 128;

            if (v->panel_icons[i])
                g->image(g, v->panel_icons[i], HM_ICON_W, HM_ICON_H, HM_ICON_W, x0 + 18, y + 4, 64,
                         36, p);
            if (v->panel_items[i])
                g->text(g, ui->title, tx, base, v->panel_items[i], v->panel_values[i] ? 150 : w - 44,
                        on ? C_INK : C_DIM, p);
            if (v->panel_values[i])
                hm_text_right(g, ui->body, x0 + w - 22, base, v->panel_values[i], on ? C_INK2 : C_SUB,
                              p);
        }
        if (v->panel_hint) {
            int tw = pw_text_width(ui->body, v->panel_hint);

            g->text(g, ui->body, x0 + w - 20 - tw, y0 + h - 12, v->panel_hint, 0, C_SUB, p);
            g->button(g, ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, x0 + w - 32 - tw,
                      y0 + h - 16, 12, C_SUB, p);
        }
    }
}

static void sheet(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int p = hm_clampi(v->sheet, 0, 256), w = 400, h = 112, x0 = (HM_W - w) / 2,
        y0 = (HM_H - h) / 2 + (256 - p) * 12 / 256;

    g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, p * 170 / 256);
    g->rrect(g, x0, y0, w, h, 16, 0, C_CARD, p * 245 / 256);
    g->icon(g, PW_ICON_CARD, x0 + 30, y0 + 30, 14, C_TEXT, p);
    if (v->sheet_title)
        g->text(g, ui->title, x0 + 52, y0 + 36, v->sheet_title, 0, C_TEXT, p);
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

        g->text(g, ui->body, x, y0 + h - 14, v->sheet_hint, 0, C_SUB, p);
        g->button(g, ui->cross_enter ? PW_BTN_CIRCLE : PW_BTN_CROSS, x - 12, y0 + h - 18, 12, C_SUB,
                  p);
        if (v->sheet_again) { /* Enter, to the left of the way out */
            x -= 30 + pw_text_width(ui->body, v->sheet_again);
            g->text(g, ui->body, x, y0 + h - 14, v->sheet_again, 0, C_TEXT, p);
            g->button(g, ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, x - 12, y0 + h - 18, 12,
                      C_TEXT, p);
        }
    }
}

/* ---- the screen ---- */

void hm_draw_modern(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int a = hm_clampi(v->appear, 0, 256);

    pills(g, ui, v, a, status(g, ui, v, a) - 12);
    if (v->tab == HM_TAB_GAMES)
        games(g, ui, v, a);
    else if (v->tab == HM_TAB_PHOTOS)
        photos(g, ui, v, a);
    else if (v->tab == HM_TAB_MUSIC)
        music(g, ui, v, a);
    else
        list(g, ui, v->rows, v->nrows, v->row_sel, v->row_pos, box_of(v->tab, v->rows, v->nrows), a,
             v->t_ms, -1, 0);
    hm_hints_right(g, ui, v, HM_W - 14, 264, C_SUB, a);
    if (v->panel > 0)
        panel(g, ui, v);
    if (v->sheet > 0)
        sheet(g, ui, v);
}

void hm_prefetch_modern(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    /* the games next to the chosen one, nearest first, as games() draws them */
    for (int d = 1; d <= 3 && v->tab != HM_TAB_PHOTOS; d++)
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
    /* the sections' rows, as list() draws them: around the cursor in this one, the first ones of
       the others (a long list - the music - is not made all at once) */
    for (int k = 0; k < HM_TABS; k++) {
        const hm_row *rows = v->other_rows[k];
        int n = v->other_nrows[k], from = k == v->tab ? v->row_sel - 8 : 0,
            to = k == v->tab ? v->row_sel + 8 : 8;
        box b;

        if (!rows)
            continue;
        if (k == HM_TAB_MUSIC) { /* its list starts at the second row */
            rows++;
            n--;
            from--;
            to--;
        }
        b = box_of(k, rows, n);
        for (int i = from < 0 ? 0 : from; i < n && i <= to; i++) {
            const hm_row *r = &rows[i];
            int lx, lw;

            label_room(ui, r, b, k == HM_TAB_MUSIC && i == v->mu_cur, &lx, &lw);
            if (!g->warm(g, r->kind == HM_ROW_HEADER || r->kind == HM_ROW_NOTE ? ui->body : ui->title,
                         r->label ? r->label : "", lw) ||
                (r->value && !g->warm(g, ui->body, r->value, b.stack ? lw : 0)))
                return;
        }
    }
    for (int i = 0; i < v->panel_count; i++)
        if (v->panel_items[i] && !g->warm(g, ui->title, v->panel_items[i], 0))
            return;
}
