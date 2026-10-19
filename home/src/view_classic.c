// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Classic: the XMB (docs/ui/styles.md, ADR-033). The month's sky with its wave (or the chosen
 * game's PIC1, sharp, as the XMB shows it); the sections as the XMB's categories - the system's
 * own icons in a row, the open one at (109, 70) with its name below; under it the open section's
 * column: games and photos as the XMB's game list (the chosen icon 144 x 80, the others 81 x 45,
 * the ones before it above the row of categories), music and play time as items with an icon
 * and a line, Settings and System as the XMB's option list (the categories slide away to the
 * left, the chosen row on the focus bar; a choice's options in a panel on the right). Metrics
 * from 480 x 272 captures (ui/views.c) and VitaXMB's measurements
 * (docs/research/m4-native-look.md).
 */
#include <string.h>

#include "view_internal.h"

#define CAT_X       109 /* the open category's centre */
#define CAT_Y       70
#define CAT_PITCH   80
#define CAT_GAP     5 /* more room each side of the open one */
#define CAT_LABEL_Y 101
#define CAT_DEEP    69 /* the bar slides left this far for an option list */

#define FOLDER_X 92 /* the game list: icons' centre */
#define FOLDER_Y 136
#define BELOW_Y  206 /* the first one after the chosen one */
#define ABOVE_GY 23  /* the one before it: above the categories */
#define SMALL_W  81
#define SMALL_H  45
#define FOLDER_PITCH 53
#define LABEL_X  176

#define ICON_X     104 /* items with an icon (Music, Play Time) */
#define TEXT_X     150
#define SEL_Y      135
#define ITEM_PITCH 56
#define ABOVE_Y    20
#define TEXT_MAX   (HM_W - TEXT_X - 6)
#define DIM_ALPHA  120

#define OPT_X0    84 /* the option list (Settings, System) */
#define OPT_TEXT  94
#define OPT_X1    466
#define OPT_PITCH 26

#define POP_W     176 /* a choice's options */
#define POP_PITCH 24

/* The XMB's text: white with a soft shadow down and to the right. */
static void stext(hm_gfx *g, pw_font *f, int x, int y, const char *s, int max_w, unsigned int color,
                  int alpha)
{
    if (!s || !s[0] || alpha <= 0)
        return;
    g->text(g, f, x + 1, y + 1, s, max_w, C_BLACK, alpha * 110 / 256);
    g->text(g, f, x, y, s, max_w, color, alpha);
}

static void stext_right(hm_gfx *g, pw_font *f, int x1, int y, const char *s, unsigned int color,
                        int alpha)
{
    if (s)
        stext(g, f, x1 - pw_text_width(f, s), y, s, 0, color, alpha);
}

static void stext_center(hm_gfx *g, pw_font *f, int cx, int y, const char *s, int max_w,
                         unsigned int color, int alpha)
{
    int w;

    if (!s)
        return;
    w = pw_text_width(f, s);
    if (max_w > 0 && w > max_w)
        w = max_w;
    stext(g, f, cx - w / 2, y, s, max_w, color, alpha);
}

static int is_options(int tab) /* an option list: the categories make room */
{
    return tab == HM_TAB_SETTINGS || tab == HM_TAB_SYSTEM;
}

/* How far the bar has slid for an option list (0-256), following tab_pos. */
static int deep(const hm_view *v)
{
    int d = 0;

    for (int i = 0; i < HM_TABS; i++)
        if (is_options(i))
            d += hm_near(i * 256 - v->tab_pos);
    return hm_clampi(d, 0, 256);
}

/* ---- the top right: the clock, the battery ---- */

static void status(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    const pw_image *b = ui->art && ui->art->battery.px ? &ui->art->battery : 0;

    if (v->battery >= 0 && b && b->h >= 4 && b->h % 4 == 0) {
        /* the XMB's own picture: four frames, full to empty; charging, it fills up in a loop */
        int fh = b->h / 4,
            frame = v->battery > 66 ? 0 : v->battery > 33 ? 1 : v->battery > 5 ? 2 : 3;

        if (v->charging)
            frame = 3 - (int)((v->t_ms / 600) % 4);
        g->image(g, b->px + frame * fh * ((b->w + 15) & ~15), b->w, fh, (b->w + 15) & ~15,
                 463 - b->w / 2, 12 - fh / 2, b->w, fh, a);
    } else if (v->battery >= 0) {
        int bars = v->battery > 66 ? 3 : v->battery > 33 ? 2 : v->battery > 5 ? 1 : 0;

        g->rrect(g, 451, 7, 21, 10, 2, PW_FX(1) + 4, C_TEXT, a);
        g->fill(g, 472, 10, 2, 4, C_TEXT, a);
        for (int i = 0; i < bars; i++)
            g->fill(g, 453 + i * 6, 9, 5, 6, v->charging ? PW_RGB(140, 230, 140) : C_TEXT, a);
    }
    if (v->clock && v->clock[0])
        stext_right(g, ui->body, 444, 17, v->clock, C_TEXT, a);
}

/* ---- the categories ---- */

static void categories(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    int dp_open = deep(v), bar_x = CAT_X - CAT_DEEP * dp_open / 256,
        /* the game list's chosen icon reaches up to where the name would be: none there */
        named = 256 - hm_near(HM_TAB_GAMES * 256 - v->tab_pos) -
                hm_near(HM_TAB_PHOTOS * 256 - v->tab_pos);

    for (int i = 0; i < HM_TABS; i++) {
        int dp = i * 256 - v->tab_pos, near = hm_near(dp),
            x = bar_x + dp * CAT_PITCH / 256 + hm_clampi(dp, -256, 256) * CAT_GAP / 256,
            ca = a * hm_lerp(200, 256, near) / 256;
        const pw_image *art = ui->art && ui->art->cat[i].px ? &ui->art->cat[i] : 0;

        ca = ca * hm_lerp(256 - dp_open, 256, near) / 256; /* the others leave room for the
                                                              option list */
        if (x < -40 || x > HM_W + 40 || ca <= 0)
            continue;
        if (art)
            hm_art_draw(g, art, x, CAT_Y, ca);
        else
            g->icon(g, hm_icon_of_tab(i), x, CAT_Y, 44, C_TEXT, ca);
        if (near > 0 && named > 0 && v->tab_names[i])
            stext_center(g, ui->body, x, CAT_LABEL_Y, v->tab_names[i], 120, C_TEXT,
                         a * near / 256 * named / 256);
    }
}

/* ---- Games and Photos: the XMB's game list ---- */

static int folder_y(int p) /* the centre of an icon p (1/256) from the cursor */
{
    if (p >= 256)
        return BELOW_Y + (p - 256) * FOLDER_PITCH / 256;
    if (p >= 0)
        return hm_lerp(FOLDER_Y, BELOW_Y, p);
    if (p >= -256)
        return hm_lerp(FOLDER_Y, ABOVE_GY, -p);
    return ABOVE_GY + (p + 256) * FOLDER_PITCH / 256;
}

static void folder(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    const hm_tile *sel;
    int near_sel;

    if (v->count <= 0) {
        if (v->empty)
            stext(g, ui->title, TEXT_X, SEL_Y + 5, v->empty, TEXT_MAX, C_TEXT, a);
        return;
    }
    for (int i = 0; i < v->count; i++) {
        int p = i * 256 - v->selpos, near = hm_near(p), y = folder_y(p),
            w = hm_lerp(SMALL_W, HM_ICON_W, near), h = hm_lerp(SMALL_H, HM_ICON_H, near);

        if (y + h / 2 < 0 || y - h / 2 > HM_H)
            continue;
        hm_tile_draw(g, v->tiles[i].icon, FOLDER_X - w / 2, y - h / 2, w, h,
                     a * (215 + 41 * near / 256) / 256);
    }
    sel = &v->tiles[hm_clampi(v->sel, 0, v->count - 1)];
    near_sel = hm_near(v->sel * 256 - v->selpos);
    if (near_sel > 128) { /* the chosen one's name, a rule, its line: as the list settles */
        int la = a * (near_sel - 128) / 128;

        stext(g, ui->title, LABEL_X, FOLDER_Y - 8, sel->title, HM_W - 6 - LABEL_X, C_TEXT, la);
        g->fill(g, LABEL_X - 1, FOLDER_Y, HM_W - 6 - LABEL_X, 1, C_TEXT, la * 200 / 256);
        if (sel->meta)
            stext(g, ui->body, LABEL_X, FOLDER_Y + 15, sel->meta, HM_W - 6 - LABEL_X, C_TEXT,
                  la * 220 / 256);
    }
}

/* ---- Music, Play Time: items with an icon and a line ---- */

static int row_y(int p) /* the centre of the row p (1/256) from the cursor */
{
    if (p >= 0)
        return SEL_Y + ITEM_PITCH * p / 256;
    if (p >= -256)
        return SEL_Y + (SEL_Y - ABOVE_Y) * p / 256;
    return ABOVE_Y + ITEM_PITCH * (p + 256) / 256;
}

static void items(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    for (int i = 0; i < v->nrows; i++) {
        const hm_row *r = &v->rows[i];
        int p = i * 256 - v->row_pos, near = hm_near(p), y = row_y(p),
            ia = a * (DIM_ALPHA + (256 - DIM_ALPHA) * near / 256) / 256, icon;
        const char *line = r->value;

        if (y < -30 || y > HM_H + 30)
            continue;
        if (r->kind == HM_ROW_HEADER || r->kind == HM_ROW_NOTE) {
            stext(g, ui->body, TEXT_X, y + 4, r->label, TEXT_MAX, C_TEXT, ia * 200 / 256);
            continue;
        }
        if (r->icon) { /* a game's ICON0, as the XMB's lists show it */
            int w = hm_lerp(54, 72, near), h = hm_lerp(30, 40, near);

            hm_tile_draw(g, r->icon, ICON_X - w / 2, y - h / 2, w, h, ia);
        } else {
            icon = v->tab == HM_TAB_PLAYTIME ? PW_ICON_WATCH
                   : v->tab == HM_TAB_MUSIC && i == 0 && v->nrows > 1
                       ? (v->mu_state == 1 ? HM_ICON_PAUSE : HM_ICON_PLAY)
                       : HM_ICON_MUSIC;
            g->icon(g, icon, ICON_X, y, 30 + 8 * near / 256,
                    v->tab == HM_TAB_MUSIC && i == v->mu_cur + 1 && i > 0 ? PW_RGB(150, 210, 255)
                                                                          : C_TEXT,
                    ia);
        }
        if (v->tab == HM_TAB_MUSIC && i == 0 && v->mu_time)
            line = v->mu_time;
        if (line) {
            stext(g, ui->title, TEXT_X, y - 1, r->label, TEXT_MAX, C_TEXT, ia);
            stext(g, ui->body, TEXT_X, y + 15, line, TEXT_MAX, C_TEXT, ia * 210 / 256);
        } else {
            stext(g, ui->title, TEXT_X, y + 5, r->label, TEXT_MAX, C_TEXT, ia);
        }
        if (r->bar >= 0 && near > 0) { /* the most played, the track's time: a thin bar */
            int bw = 160;

            g->fill(g, TEXT_X, y + 21, bw, 2, C_TEXT, ia * 70 / 256);
            g->fill(g, TEXT_X, y + 21, bw * hm_clampi(r->bar, 0, 256) / 256, 2, C_TEXT, ia);
        }
    }
}

/* ---- Settings, System: the XMB's option list ---- */

static void options(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    int dp = deep(v), oa = a * dp / 256, slide = (256 - dp) * 40 / 256,
        va = 256 - hm_clampi(v->popup, 0, 256); /* the values give way to a choice's options */

    if (oa <= 0)
        return;
    if (v->row_sel >= 0 && v->row_sel < v->nrows) { /* the focus bar, where the cursor rests */
        g->fill(g, OPT_X0 + slide, SEL_Y - 12, OPT_X1 + 6 - OPT_X0, 24, C_TEXT, oa * 34 / 256);
        g->fill(g, OPT_X0 + slide, SEL_Y - 12, OPT_X1 + 6 - OPT_X0, 1, C_TEXT, oa * 90 / 256);
        g->fill(g, OPT_X0 + slide, SEL_Y + 11, OPT_X1 + 6 - OPT_X0, 1, C_TEXT, oa * 90 / 256);
    }
    for (int i = 0; i < v->nrows; i++) {
        const hm_row *r = &v->rows[i];
        int y = SEL_Y + (i * 256 - v->row_pos) * OPT_PITCH / 256, on = i == v->row_sel, ra;

        if (y < 30 || y > 252)
            continue;
        ra = oa * hm_clampi(y < 56 ? (y - 30) * 256 / 26 : y > 226 ? (252 - y) * 256 / 26 : 256, 0,
                            256) /
             256;
        if (r->kind == HM_ROW_HEADER || r->kind == HM_ROW_NOTE) {
            stext(g, ui->body, OPT_TEXT + slide, y + 4, r->label, OPT_X1 - OPT_TEXT, C_TEXT,
                  ra * 170 / 256);
            continue;
        }
        stext(g, ui->title, OPT_TEXT + slide, y + 5, r->label, r->value ? 220 : OPT_X1 - OPT_TEXT,
              C_TEXT, ra * (on ? 256 : 190) / 256);
        if (r->value && va > 0)
            stext_right(g, ui->body, OPT_X1 + slide, y + 4, r->value, C_TEXT,
                        ra * (on ? 256 : 170) / 256 * va / 256);
        else if (r->kind == HM_ROW_ACTION && on)
            hm_chevron(g, OPT_X1 - 4 + slide, y, 1, C_TEXT, ra);
    }
}

static void hints(hm_gfx *g, const hm_ui *ui, const char *enter, const char *back,
                  const char *options, int a);

/* A choice's options: the XMB's panel sliding in from the right. */
static void popup(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int p = hm_clampi(v->popup, 0, 256), x0 = HM_W - POP_W * p / 256;

    g->fill(g, x0, 0, POP_W, HM_H, C_BLACK, p * 205 / 256);
    g->fill(g, x0, 0, 1, HM_H, C_TEXT, p * 70 / 256);
    g->fill(g, x0 + 8, SEL_Y - 12, POP_W - 16, 24, C_TEXT, p * 40 / 256);
    for (int k = 0; k < v->popup_count; k++) {
        int y = SEL_Y + (k * 256 - v->popup_pos) * POP_PITCH / 256;

        if (y < 10 || y > HM_H - 10)
            continue;
        stext(g, ui->title, x0 + 20, y + 5, v->popup_items[k], POP_W - 30, C_TEXT,
              p * (k == v->popup_sel ? 256 : 160) / 256);
    }
    hints(g, ui, v->hint_enter, v->hint_back, 0, p);
}

/* ---- the hints: centred at the bottom, the system's button pictures ---- */

static void hints(hm_gfx *g, const hm_ui *ui, const char *enter, const char *back,
                  const char *options, int a)
{
    const char *label[3] = {enter, options, back};
    int btn[3] = {ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, PW_BTN_TRIANGLE,
                  ui->cross_enter ? PW_BTN_CIRCLE : PW_BTN_CROSS},
        w = 0, x;

    for (int i = 0; i < 3; i++)
        if (label[i])
            w += 14 + pw_text_width(ui->body, label[i]) + 16;
    x = HM_W / 2 - (w - 16) / 2;
    for (int i = 0; i < 3; i++) {
        if (!label[i])
            continue;
        hm_button_pic(g, ui, btn[i], x + 6, 256, 12, C_TEXT, a);
        stext(g, ui->body, x + 14, 260, label[i], 0, C_TEXT, a);
        x += 14 + pw_text_width(ui->body, label[i]) + 16;
    }
}

/* ---- over everything: the HOME panel, a message ---- */

static void panel(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int pics = 0, wide = 0;

    for (int i = 0; i < v->panel_count; i++) {
        pics |= v->panel_icons[i] != 0;
        wide |= v->panel_values[i] != 0;
    }
    {
        int p = hm_clampi(v->panel, 0, 256), n = v->panel_count, rh = pics ? 44 : 24,
            h = 36 + n * rh + 10, top = (HM_H - h) / 2 - 6, tx = pics ? 140 : 96;

        g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, p * 150 / 256);
        g->fill(g, 0, top, HM_W, h, C_BLACK, p * 120 / 256);
        g->fill(g, 40, top, HM_W - 80, 1, C_TEXT, p);
        g->fill(g, 40, top + h, HM_W - 80, 1, C_TEXT, p);
        if (v->panel_title)
            stext_center(g, ui->title, HM_W / 2, top + 22, v->panel_title, HM_W - 100, C_TEXT, p);
        g->fill(g, 60, top + 34 + v->panel_pos * rh / 256, HM_W - 120, rh - 2,
                PW_RGB(0xCF, 0xCF, 0xCF), p * 90 / 256);
        for (int i = 0; i < n; i++) {
            int y = top + 34 + i * rh, base = y + rh / 2 + 4;

            int vw = v->panel_values[i] ? pw_text_width(ui->body, v->panel_values[i]) + 14 : 0;

            if (v->panel_icons[i])
                g->image(g, v->panel_icons[i], HM_ICON_W, HM_ICON_H, HM_ICON_W, 62, y + 3, 68, 38,
                         p);
            if (!pics && !wide) {
                stext_center(g, ui->title, HM_W / 2, base, v->panel_items[i], HM_W - 140, C_TEXT,
                             p);
            } else if (pics && v->panel_values[i]) { /* a state: its name, its time under it */
                stext(g, ui->title, tx, y + 18, v->panel_items[i], HM_W - 60 - tx, C_TEXT, p);
                stext(g, ui->body, tx, y + 34, v->panel_values[i], HM_W - 60 - tx, C_TEXT,
                      p * 210 / 256);
            } else {
                stext(g, ui->title, tx, base, v->panel_items[i], HM_W - 60 - vw - tx, C_TEXT, p);
                if (v->panel_values[i])
                    stext_right(g, ui->body, HM_W - 60, base, v->panel_values[i], C_TEXT, p);
            }
        }
        hints(g, ui, v->panel_hint ? v->panel_hint : 0, 0, 0, p);
    }
}

static void sheet(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int p = hm_clampi(v->sheet, 0, 256);

    g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, p * 170 / 256);
    g->fill(g, 60, 92, HM_W - 120, 1, C_TEXT, p);
    g->fill(g, 60, 183, HM_W - 120, 1, C_TEXT, p);
    stext_center(g, ui->title, HM_W / 2, 122, v->sheet_title, HM_W - 140, C_TEXT, p);
    stext_center(g, ui->body, HM_W / 2, 146, v->sheet_text, HM_W - 140, C_TEXT, p);
    if (v->sheet_busy) {
        int head = (int)(v->t_ms / 90) % 8;

        for (int i = 0; i < 8; i++) { /* eight dots, the brightest one turning */
            static const signed char k[8][2] = {{0, -8}, {6, -6}, {8, 0},  {6, 6},
                                                {0, 8},  {-6, 6}, {-8, 0}, {-6, -6}};
            int age = (head - i + 8) % 8;

            g->rrect(g, HM_W / 2 + k[i][0] - 2, 166 + k[i][1] - 2, 4, 4, 2, 0, C_TEXT,
                     p * (256 - age * 28) / 256);
        }
    }
    hints(g, ui, v->sheet_again, v->sheet_hint, 0, p);
}

/* ---- the screen ---- */

void hm_draw_classic(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int a = hm_clampi(v->appear, 0, 256);

    if (v->wave > 0)
        g->wave(g, v->t_ms, v->wave * a / 256);
    categories(g, ui, v, a);
    if (v->tab == HM_TAB_GAMES || v->tab == HM_TAB_PHOTOS)
        folder(g, ui, v, a);
    else if (is_options(v->tab))
        options(g, ui, v, a);
    else
        items(g, ui, v, a);
    status(g, ui, v, a);
    if (v->popup > 0)
        popup(g, ui, v);
    if (v->panel > 0)
        panel(g, ui, v);
    if (v->sheet > 0)
        sheet(g, ui, v);
}

void hm_prefetch_classic(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    for (int d = 1; d <= 3; d++) /* the games next to the chosen one */
        for (int s = -1; s <= 1; s += 2) {
            int i = v->sel + s * d;

            if (i < 0 || i >= v->count)
                continue;
            if (!g->warm(g, ui->title, v->tiles[i].title ? v->tiles[i].title : "",
                         HM_W - 6 - LABEL_X) ||
                (v->tiles[i].meta && !g->warm(g, ui->body, v->tiles[i].meta, HM_W - 6 - LABEL_X)))
                return;
        }
    for (int k = 0; k < HM_TABS; k++) { /* the sections' rows near where they will show */
        const hm_row *rows = v->other_rows[k];
        int n = v->other_nrows[k], from = k == v->tab ? v->row_sel - 6 : 0,
            to = k == v->tab ? v->row_sel + 8 : 6;

        for (int i = from < 0 ? 0 : from; rows && i < n && i <= to; i++) {
            const hm_row *r = &rows[i];
            pw_font *f = r->kind == HM_ROW_HEADER || r->kind == HM_ROW_NOTE ? ui->body : ui->title;
            int w = is_options(k) ? (r->kind == HM_ROW_HEADER || r->kind == HM_ROW_NOTE
                                         ? OPT_X1 - OPT_TEXT
                                     : r->value ? 220
                                                : OPT_X1 - OPT_TEXT)
                                  : TEXT_MAX;

            if (!g->warm(g, f, r->label ? r->label : "", w) ||
                (r->value && !g->warm(g, ui->body, r->value, is_options(k) ? 0 : TEXT_MAX)))
                return;
        }
    }
}
