// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Minimal (docs/ui/styles.md): nothing but text over the darkened background - the sections as
 * words at the top left (the chosen one white, a line gliding under it), one column of names
 * whose chosen line rests in the middle (bright, a short rule under it, its note below), "name ...
 * value" for the settings, a page's name small above its column (ADR-035); what plays as words
 * and a thin line; the clock small and grey. The theme's colours (hm_palette).
 */
#include <string.h>

#include "stb_sprintf.h"
#include "view_internal.h"

#define COL_X    40
#define COL_X1   440
#define CUR_Y    128 /* the chosen line's baseline */
#define PITCH    24
#define NOTE_GAP 20 /* room under the chosen game for its note */
#define SLIDE    110

static void top(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_view *v, int a)
{
    int x[HM_TABS + 1], w[HM_TABS], from, to, f, shift = 0, right = HM_W - 16;
    unsigned int acc = ui->accent ? ui->accent : C_ON;
    char pct[8];

    if (v->clock && v->clock[0]) {
        hm_text_right(g, ui->body, right, 24, v->clock, p->sub, a);
        right -= pw_text_width(ui->body, v->clock) + 12;
    }
    if (v->battery >= 0) {
        if (v->battery_pct)
            stbsp_snprintf(pct, sizeof(pct), "%d%%", hm_clampi(v->battery, 0, 100));
        else
            stbsp_snprintf(pct, sizeof(pct), "%s",
                           v->battery > 66   ? "|||"
                           : v->battery > 33 ? "||"
                                             : "|");
        hm_text_right(g, ui->body, right, 24, pct, v->charging ? PW_RGB(110, 220, 120) : p->sub, a);
        right -= pw_text_width(ui->body, pct) + 12;
    }
    x[0] = 20;
    for (int i = 0; i < HM_TABS; i++) {
        w[i] = v->tab_names[i] ? pw_text_width(ui->body, v->tab_names[i]) : 0;
        x[i + 1] = x[i] + w[i] + 16;
    }
    from = hm_clampi(v->tab_pos / 256, 0, HM_TABS - 1);
    to = hm_clampi(from + 1, 0, HM_TABS - 1);
    f = v->tab_pos - from * 256;
    if (x[HM_TABS] - 16 > right - 8) /* too wide: the chosen one stays in view */
        shift = hm_clampi(hm_lerp(x[from] + w[from], x[to] + w[to], f) + 8 - (right - 8), 0, 1000);
    for (int i = 0; i < HM_TABS; i++) {
        int tx = x[i] - shift;

        if (v->tab_names[i] && tx >= 4 && tx + w[i] <= right)
            g->text(g, ui->body, tx, 24, v->tab_names[i], 0, i == v->tab ? p->text : p->sub,
                    a * (i == v->tab ? 256 : 140) / 256);
    }
    g->fill(g, hm_lerp(x[from], x[to], f) - shift, 29, hm_lerp(w[from], w[to], f), 1,
            ui->theme == HM_THEME_DARK ? p->text : acc, a);
}

/* The baseline of a line p (1/256) from the chosen one; gap: the room under the chosen one. */
static int line_y(int p, int gap)
{
    return CUR_Y + p * PITCH / 256 + (p > 0 ? gap * hm_clampi(p, 0, 256) / 256 : 0);
}

static int line_alpha(int p, int a)
{
    int d = p < 0 ? -p : p;

    return a * hm_clampi(256 - d * 46 / 256, 40, 256) / 256;
}

/* A page's name, small and grey above the column. */
static void heading(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_page *pg, int a)
{
    if (pg->title) { /* the name first; its line in what is left */
        int tw = pw_text_width(ui->body, pg->title), w = COL_X1 - COL_X,
            room = w - (tw < w * 3 / 5 ? tw : w * 3 / 5) - 24;

        g->text(g, ui->body, COL_X, 52, pg->title, w * 3 / 5, p->sub, a);
        if (pg->sub && room > 50) {
            int sw = pw_text_width(ui->body, pg->sub);

            g->text(g, ui->body, COL_X1 - (sw < room ? sw : room), 52, pg->sub, room, p->sub,
                    a * 180 / 256);
        }
    }
}

static void names(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_page *pg, int a)
{
    const hm_tile *sel;
    int near;

    heading(g, ui, p, pg, a);
    if (pg->count <= 0) {
        if (pg->empty)
            g->text(g, ui->title, COL_X, CUR_Y, pg->empty, COL_X1 - COL_X, p->sub, a);
        return;
    }
    for (int i = 0; i < pg->count; i++) {
        int t = i * 256 - pg->selpos, y = line_y(t, NOTE_GAP);

        if (y < 64 || y > 246)
            continue;
        g->text(g, ui->title, COL_X, y, pg->tiles[i].title ? pg->tiles[i].title : "",
                COL_X1 - COL_X, i == pg->sel ? p->text : p->sub, line_alpha(t, a));
        if (pg->tiles[i].fav) /* a favourite: a small star before it */
            g->icon(g, HM_ICON_STAR, COL_X - 14, y - 5, 10, PW_RGB(255, 206, 64), line_alpha(t, a));
    }
    sel = &pg->tiles[hm_clampi(pg->sel, 0, pg->count - 1)];
    near = hm_near(pg->sel * 256 - pg->selpos);
    g->fill(g, COL_X, CUR_Y + 6, 24, 1, p->text, a * near / 256);
    if (sel->meta)
        g->text(g, ui->body, COL_X, CUR_Y + 22, sel->meta, COL_X1 - COL_X, p->sub,
                a * near / 256 * 200 / 256);
}

static void lines(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_view *v,
                  const hm_page *pg, int a)
{
    unsigned int acc = ui->accent ? ui->accent : C_ON;

    heading(g, ui, p, pg, a);
    for (int i = 0; i < pg->nrows; i++) {
        const hm_row *r = &pg->rows[i];
        int t = i * 256 - pg->row_pos, y = line_y(t, 0), on = i == pg->row_sel,
            la = line_alpha(t, a);
        const char *value = r->value;

        if (y < 64 || y > 246)
            continue;
        if (pg->layout == HM_PAGE_MUSIC && i == 0 && v->mu_time)
            value = v->mu_time;
        if (r->kind == HM_ROW_HEADER || r->kind == HM_ROW_NOTE) {
            g->text(g, ui->body, COL_X, y, r->label ? r->label : "", COL_X1 - COL_X, p->sub,
                    la * 180 / 256);
            continue;
        }
        g->text(g, ui->title, COL_X, y, r->label ? r->label : "",
                value ? (COL_X1 - COL_X) * 3 / 5 : COL_X1 - COL_X - 16, on ? p->text : p->sub, la);
        if (value) {
            int x1 = COL_X1 - (on && r->kind == HM_ROW_CHOICE ? 14 : 0) -
                     (r->kind == HM_ROW_LINK ? 14 : 0);

            hm_text_right(g, ui->body, x1, y, value, on ? p->text : p->sub, la);
            if (r->swatch)
                g->rrect(g, x1 - pw_text_width(ui->body, value) - 14, y - 8, 8, 8, 4, 0, r->swatch,
                         la);
            if (on && r->kind == HM_ROW_CHOICE) { /* the ways to change it */
                hm_chevron(g, COL_X1 - 4, y - 4, 1, p->text, la);
                hm_chevron(g, x1 - pw_text_width(ui->body, value) - 10 - (r->swatch ? 14 : 0),
                           y - 4, -1, p->text, la);
            }
        }
        if (r->kind == HM_ROW_LINK)
            hm_chevron(g, COL_X1 - 4, y - 4, 1, on ? p->text : p->sub, la);
        if (on)
            g->fill(g, COL_X, y + 6, 24, 1, ui->theme == HM_THEME_DARK ? p->text : acc,
                    a * hm_near(t) / 256);
        if (r->bar >= 0 && on) {
            g->fill(g, COL_X + 32, y + 6, 120, 1, p->text, la * 60 / 256);
            g->fill(g, COL_X + 32, y + 6, 120 * hm_clampi(r->bar, 0, 256) / 256, 1, p->text, la);
        }
    }
}

/* What plays: words in the middle, a thin line for the time, the controls as small signs. */
static void now(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_view *v,
                const hm_page *pg, int a)
{
    int w = 300, x0 = HM_W / 2 - w / 2;
    unsigned int acc = ui->accent ? ui->accent : C_ON;

    heading(g, ui, p, pg, a);
    if (!v->mu_title) {
        hm_text_center(g, ui->title, HM_W / 2, 130, pg->empty, HM_W - 80, p->sub, a);
        return;
    }
    hm_text_center(g, ui->big, HM_W / 2, 104, v->mu_title, HM_W - 80, p->text, a);
    if (v->mu_artist)
        hm_text_center(g, ui->title, HM_W / 2, 126, v->mu_artist, HM_W - 80, p->sub, a);
    if (v->mu_album)
        hm_text_center(g, ui->body, HM_W / 2, 142, v->mu_album, HM_W - 80, p->sub, a * 180 / 256);
    { /* a thin line for the time, thicker with a knob while chosen */
        int f = v->mu_focus >= 0 ? v->mu_bar_t : 0, th = 1 + 2 * f / 256,
            k = w * hm_clampi(v->mu_bar < 0 ? 0 : v->mu_bar, 0, 256) / 256;
        hm_player_look look = {48, 12, 14, 0, p->text, p->dim, acc, p->text, 26, 236, p->sub};

        g->fill(g, x0, 164 - th / 2, w, th, p->text, a * 60 / 256);
        if (v->mu_bar >= 0) {
            g->fill(g, x0, 164 - th / 2, k, th, p->text, a);
            if (f > 0)
                g->rrect(g, x0 + k - 4, 160, 8, 8, 4, 0, p->text, a * f / 256);
        }
        if (v->mu_time)
            g->text(g, ui->body, x0, 181, v->mu_time, 0, p->sub, a);
        hm_text_right(g, ui->body, x0 + w, 181, v->mu_length, p->sub, a);
        hm_player(g, ui->body, v, &look, HM_W / 2, 208, a);
    }
}

static void page(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_view *v,
                 const hm_page *pg, int a)
{
    switch (pg->layout) {
    case HM_PAGE_GAMES:
    case HM_PAGE_PHOTOS:
    case HM_PAGE_ALBUMS:
    case HM_PAGE_SAVES:
        names(g, ui, p, pg, a);
        break;
    case HM_PAGE_NOW:
        now(g, ui, p, v, pg, a);
        break;
    default:
        lines(g, ui, p, v, pg, a);
        break;
    }
}

static void hints(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_view *v, int a)
{
    int w = 0, x;
    const char *label[7] = {v->hint_enter,
                            v->hint_options,
                            v->hint_square,
                            v->hint_back,
                            v->hint_lr,
                            v->hint_ud,
                            v->depth == 0 ? v->hint_tabs : 0};
    int btn[7] = {ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE,
                  PW_BTN_TRIANGLE,
                  PW_BTN_SQUARE,
                  ui->cross_enter ? PW_BTN_CIRCLE : PW_BTN_CROSS,
                  -2,
                  -3,
                  -1};

    for (int i = 0; i < 7; i++)
        if (label[i])
            w += (btn[i] != -1 ? 14 : pw_text_width(ui->body, "L R ")) +
                 pw_text_width(ui->body, label[i]) + 18;
    x = HM_W / 2 - (w - 18) / 2;
    for (int i = 0; i < 7; i++) {
        if (!label[i])
            continue;
        if (btn[i] >= 0 || btn[i] < -1) {
            if (btn[i] >= 0)
                g->button(g, btn[i], x + 5, 260, 10, p->sub, a * 200 / 256);
            else
                g->icon(g, btn[i] == -2 ? HM_ICON_LR : HM_ICON_UD, x + 5, 260, 10, p->sub,
                        a * 200 / 256);
            x += 14;
        } else {
            g->text(g, ui->body, x, 264, "L R", 0, p->sub, a * 200 / 256);
            x += pw_text_width(ui->body, "L R ");
        }
        g->text(g, ui->body, x, 264, label[i], 0, p->sub, a * 200 / 256);
        x += pw_text_width(ui->body, label[i]) + 18;
    }
}

static void panel(hm_gfx *g, const hm_ui *ui, const hm_palette *pal, const hm_view *v)
{
    int p = hm_clampi(v->panel, 0, 256), n = v->panel_count, y0 = HM_H / 2 - n * PITCH / 2 + 8;
    unsigned int text = ui->theme == HM_THEME_PAPER ? C_TEXT : pal->text, sub = C_SUB;

    g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, p * 232 / 256);
    if (v->panel_title)
        hm_text_center(g, ui->body, HM_W / 2, y0 - 26, v->panel_title, HM_W - 80, sub, p);
    for (int i = 0; i < n; i++) {
        int y = y0 + i * PITCH, on = i == v->panel_sel;
        const char *s = v->panel_items[i];

        if (!s)
            continue;
        if (v->panel_values[i]) { /* a state and its time: on one line */
            g->text(g, ui->title, 120, y, s, 150, on ? text : sub, p);
            hm_text_right(g, ui->body, HM_W - 120, y, v->panel_values[i], on ? text : sub, p);
        } else {
            hm_text_center(g, ui->title, HM_W / 2, y, s, HM_W - 120, on ? text : sub, p);
        }
        if (on)
            g->fill(g, HM_W / 2 - 12, y + 6, 24, 1, text, p);
    }
    if (v->panel_hint)
        hm_text_center(g, ui->body, HM_W / 2, y0 + n * PITCH + 10, v->panel_hint, 0, sub, p);
}

static void sheet(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int p = hm_clampi(v->sheet, 0, 256);

    g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, p * 232 / 256);
    hm_text_center(g, ui->title, HM_W / 2, 118, v->sheet_title, HM_W - 80, C_TEXT, p);
    hm_text_wrap(g, ui->body, HM_W / 2, 142, 14, 2, v->sheet_text, HM_W - 80, 1, C_SUB, p);
    if (v->sheet_busy)
        for (int i = 0; i < 3; i++) {
            int t = (int)((v->t_ms / 4 + (unsigned int)(i * 60)) % 256),
                k = t < 128 ? t * 2 : (255 - t) * 2;

            g->fill(g, HM_W / 2 - 10 + i * 8, 160, 3, 3, C_TEXT, p * (60 + k * 196 / 256) / 256);
        }
    if (v->sheet_hint)
        hm_text_center(g, ui->body, HM_W / 2, 196, v->sheet_hint, 0, C_SUB, p);
    if (v->sheet_again)
        hm_text_center(g, ui->body, HM_W / 2, 212, v->sheet_again, 0, C_TEXT, p);
}

void hm_draw_minimal(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int a = hm_clampi(v->appear, 0, 256);
    hm_palette p;

    hm_palette_of(ui, &p);
    if (v->viewer < 256) {
        if (ui->theme != HM_THEME_PAPER) /* (paper is light: its text is dark) */
            g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, hm_clampi(120 + p.shade / 2 + v->dim, 0, 256));
        top(g, ui, &p, v, a);
        for (int d = 0; d < HM_DEPTH; d++) {
            int t = hm_page_t(v, d);
            hm_shift s;

            if (t <= -256 || t >= 256)
                continue;
            page(t ? hm_shift_init(&s, g, t * SLIDE / 256,
                                   (256 - (t < 0 ? -t : t)) * (256 - (t < 0 ? -t : t)) / 256)
                   : g,
                 ui, &p, v, &v->page[d], a);
        }
        hints(g, ui, &p, v, a);
    }
    if (v->panel > 0)
        panel(g, ui, &p, v);
    hm_draw_cal(g, ui, v);
    if (v->sheet > 0)
        sheet(g, ui, v);
    hm_draw_toast(g, ui, v, C_BLACK, C_TEXT);
    hm_draw_keyboard(g, ui, v, &p);
}

void hm_prefetch_minimal(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    const hm_page *pg = hm_top(v);

    for (int d = 1; d <= 4; d++)
        for (int s = -1; s <= 1; s += 2) {
            int i = pg->sel + s * d;

            if (i >= 0 && i < pg->count &&
                !g->warm(g, ui->title, pg->tiles[i].title ? pg->tiles[i].title : "",
                         COL_X1 - COL_X))
                return;
        }
    for (int k = 0; k < HM_TABS; k++) {
        int here = k == v->tab && pg->rows;
        const hm_row *rows = here ? pg->rows : v->other_rows[k];
        int n = here ? pg->nrows : v->other_nrows[k], cur = here ? pg->row_sel : 0, from = cur - 5,
            to = cur + 6;

        for (int i = from < 0 ? 0 : from; rows && i < n && i <= to; i++) {
            const hm_row *r = &rows[i];
            int head = r->kind == HM_ROW_HEADER || r->kind == HM_ROW_NOTE;

            if (!g->warm(g, head ? ui->body : ui->title, r->label ? r->label : "",
                         head       ? COL_X1 - COL_X
                         : r->value ? (COL_X1 - COL_X) * 3 / 5
                                    : COL_X1 - COL_X - 16) ||
                (r->value && !g->warm(g, ui->body, r->value, 0)))
                return;
        }
    }
}
