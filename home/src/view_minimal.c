// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Minimal (docs/ui/styles.md): nothing but text over the darkened background - the sections as
 * words at the top left (the chosen one white, a line gliding under it), one column of names
 * whose chosen line rests in the middle (bright, a short rule under it, its note below), "name ...
 * value" for the settings; the clock small and grey.
 */
#include <string.h>

#include "stb_sprintf.h"
#include "view_internal.h"

#define COL_X    40
#define COL_X1   440
#define CUR_Y    128 /* the chosen line's baseline */
#define PITCH    24
#define NOTE_GAP 20 /* room under the chosen game for its note */

static void top(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    int x[HM_TABS + 1], w[HM_TABS], from, to, f, shift = 0, right = HM_W - 16;
    char pct[8];

    if (v->clock && v->clock[0]) {
        hm_text_right(g, ui->body, right, 24, v->clock, C_SUB, a);
        right -= pw_text_width(ui->body, v->clock) + 12;
    }
    if (v->battery >= 0) {
        stbsp_snprintf(pct, sizeof(pct), "%d%%", hm_clampi(v->battery, 0, 100));
        hm_text_right(g, ui->body, right, 24, pct, v->charging ? PW_RGB(140, 230, 140) : C_SUB, a);
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
            g->text(g, ui->body, tx, 24, v->tab_names[i], 0, i == v->tab ? C_TEXT : C_SUB,
                    a * (i == v->tab ? 256 : 140) / 256);
    }
    g->fill(g, hm_lerp(x[from], x[to], f) - shift, 29, hm_lerp(w[from], w[to], f), 1, C_TEXT, a);
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

static void names(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    const hm_tile *sel;
    int near;

    if (v->count <= 0) {
        if (v->empty)
            g->text(g, ui->title, COL_X, CUR_Y, v->empty, COL_X1 - COL_X, C_SUB, a);
        return;
    }
    for (int i = 0; i < v->count; i++) {
        int p = i * 256 - v->selpos, y = line_y(p, NOTE_GAP);

        if (y < 44 || y > 246)
            continue;
        g->text(g, ui->title, COL_X, y, v->tiles[i].title ? v->tiles[i].title : "", COL_X1 - COL_X,
                i == v->sel ? C_TEXT : C_SUB, line_alpha(p, a));
    }
    sel = &v->tiles[hm_clampi(v->sel, 0, v->count - 1)];
    near = hm_near(v->sel * 256 - v->selpos);
    g->fill(g, COL_X, CUR_Y + 6, 24, 1, C_TEXT, a * near / 256);
    if (sel->meta)
        g->text(g, ui->body, COL_X, CUR_Y + 22, sel->meta, COL_X1 - COL_X, C_SUB,
                a * near / 256 * 200 / 256);
}

static void lines(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    for (int i = 0; i < v->nrows; i++) {
        const hm_row *r = &v->rows[i];
        int p = i * 256 - v->row_pos, y = line_y(p, 0), on = i == v->row_sel, la = line_alpha(p, a);
        const char *value = r->value;

        if (y < 44 || y > 246)
            continue;
        if (v->tab == HM_TAB_MUSIC && i == 0 && v->mu_time)
            value = v->mu_time;
        if (r->kind == HM_ROW_HEADER || r->kind == HM_ROW_NOTE) {
            g->text(g, ui->body, COL_X, y, r->label ? r->label : "", COL_X1 - COL_X, C_SUB,
                    la * 180 / 256);
            continue;
        }
        g->text(g, ui->title, COL_X, y, r->label ? r->label : "",
                value ? (COL_X1 - COL_X) * 3 / 5 : COL_X1 - COL_X, on ? C_TEXT : C_SUB, la);
        if (value) {
            int x1 = COL_X1 - (on && r->kind == HM_ROW_CHOICE ? 14 : 0);

            hm_text_right(g, ui->body, x1, y, value, on ? C_TEXT : C_SUB, la);
            if (on && r->kind == HM_ROW_CHOICE) { /* the ways to change it */
                hm_chevron(g, COL_X1 - 4, y - 4, 1, C_TEXT, la);
                hm_chevron(g, x1 - pw_text_width(ui->body, value) - 10, y - 4, -1, C_TEXT, la);
            }
        }
        if (on)
            g->fill(g, COL_X, y + 6, 24, 1, C_TEXT, a * hm_near(p) / 256);
        if (r->bar >= 0 && on) {
            g->fill(g, COL_X + 32, y + 6, 120, 1, C_TEXT, la * 60 / 256);
            g->fill(g, COL_X + 32, y + 6, 120 * hm_clampi(r->bar, 0, 256) / 256, 1, C_TEXT, la);
        }
    }
}

static void hints(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    int w = 0, x;
    const char *label[3] = {v->hint_enter, v->hint_options, v->hint_tabs};
    int btn[3] = {ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, PW_BTN_TRIANGLE, -1};

    for (int i = 0; i < 3; i++)
        if (label[i])
            w += (btn[i] >= 0 ? 14 : pw_text_width(ui->body, "L R ")) +
                 pw_text_width(ui->body, label[i]) + 18;
    x = HM_W / 2 - (w - 18) / 2;
    for (int i = 0; i < 3; i++) {
        if (!label[i])
            continue;
        if (btn[i] >= 0) {
            g->button(g, btn[i], x + 5, 260, 10, C_SUB, a * 200 / 256);
            x += 14;
        } else {
            g->text(g, ui->body, x, 264, "L R", 0, C_SUB, a * 200 / 256);
            x += pw_text_width(ui->body, "L R ");
        }
        g->text(g, ui->body, x, 264, label[i], 0, C_SUB, a * 200 / 256);
        x += pw_text_width(ui->body, label[i]) + 18;
    }
}

static void panel(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int p = hm_clampi(v->panel, 0, 256), n = v->panel_count, y0 = HM_H / 2 - n * PITCH / 2 + 8;

    g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, p * 232 / 256);
    if (v->panel_title)
        hm_text_center(g, ui->body, HM_W / 2, y0 - 26, v->panel_title, HM_W - 80, C_SUB, p);
    for (int i = 0; i < n; i++) {
        int y = y0 + i * PITCH, on = i == v->panel_sel;
        const char *s = v->panel_items[i];

        if (!s)
            continue;
        if (v->panel_values[i]) { /* a state and its time: on one line */
            g->text(g, ui->title, 120, y, s, 150, on ? C_TEXT : C_SUB, p);
            hm_text_right(g, ui->body, HM_W - 120, y, v->panel_values[i], on ? C_TEXT : C_SUB, p);
        } else {
            hm_text_center(g, ui->title, HM_W / 2, y, s, HM_W - 120, on ? C_TEXT : C_SUB, p);
        }
        if (on)
            g->fill(g, HM_W / 2 - 12, y + 6, 24, 1, C_TEXT, p);
    }
    if (v->panel_hint)
        hm_text_center(g, ui->body, HM_W / 2, y0 + n * PITCH + 10, v->panel_hint, 0, C_SUB, p);
}

static void sheet(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int p = hm_clampi(v->sheet, 0, 256);

    g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, p * 232 / 256);
    hm_text_center(g, ui->title, HM_W / 2, 118, v->sheet_title, HM_W - 80, C_TEXT, p);
    hm_text_center(g, ui->body, HM_W / 2, 142, v->sheet_text, HM_W - 80, C_SUB, p);
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

    g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, 120);
    top(g, ui, v, a);
    if (v->tab == HM_TAB_GAMES || v->tab == HM_TAB_PHOTOS)
        names(g, ui, v, a);
    else
        lines(g, ui, v, a);
    hints(g, ui, v, a);
    if (v->panel > 0)
        panel(g, ui, v);
    if (v->sheet > 0)
        sheet(g, ui, v);
}

void hm_prefetch_minimal(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    for (int d = 1; d <= 4; d++)
        for (int s = -1; s <= 1; s += 2) {
            int i = v->sel + s * d;

            if (i >= 0 && i < v->count &&
                !g->warm(g, ui->title, v->tiles[i].title ? v->tiles[i].title : "", COL_X1 - COL_X))
                return;
        }
    for (int k = 0; k < HM_TABS; k++) {
        const hm_row *rows = v->other_rows[k];
        int n = v->other_nrows[k], from = k == v->tab ? v->row_sel - 5 : 0,
            to = k == v->tab ? v->row_sel + 6 : 6;

        for (int i = from < 0 ? 0 : from; rows && i < n && i <= to; i++) {
            const hm_row *r = &rows[i];
            int head = r->kind == HM_ROW_HEADER || r->kind == HM_ROW_NOTE;

            if (!g->warm(g, head ? ui->body : ui->title, r->label ? r->label : "",
                         head || !r->value ? COL_X1 - COL_X : (COL_X1 - COL_X) * 3 / 5) ||
                (r->value && !g->warm(g, ui->body, r->value, 0)))
                return;
        }
    }
}
