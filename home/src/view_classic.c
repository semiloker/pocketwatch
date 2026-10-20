// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Classic: the XMB (docs/ui/styles.md, ADR-033). The month's sky with its wave (or the chosen
 * game's PIC1, sharp, as the XMB shows it); the sections as the XMB's categories - the system's
 * own icons in a row, the open one at (109, 70) with its name below; under it the open section's
 * column: games, albums, photos and saved data as the XMB's game list (the chosen icon
 * 144 x 80, the others 81 x 45, the ones before it above the row of categories), music, play
 * time and the settings' categories as items with an icon (the system's own Settings icons) and
 * a line; a category opened as the XMB's option list (the categories and the items slide away
 * to the left, the chosen row on the focus bar; a choice's options in a panel on the right); a
 * game's Triangle menu as the XMB's panel on the right (ADR-035). Metrics from 480 x 272
 * captures (ui/views.c) and VitaXMB's measurements (docs/research/m4-native-look.md).
 */
#include <string.h>

#include "stb_sprintf.h"
#include "view_internal.h"

#define CAT_X       109 /* the open category's centre */
#define CAT_Y       70
#define CAT_PITCH   80
#define CAT_GAP     5 /* more room each side of the open one */
#define CAT_LABEL_Y 101
#define CAT_DEEP    69 /* the bar slides left this far for an option list */

#define FOLDER_X     92 /* the game list: icons' centre */
#define FOLDER_Y     136
#define BELOW_Y      206 /* the first one after the chosen one */
#define ABOVE_GY     23  /* the one before it: above the categories */
#define SMALL_W      81
#define SMALL_H      45
#define FOLDER_PITCH 53
#define LABEL_X      176

#define ICON_X     104 /* items with an icon (Music, Play Time, the settings' categories) */
#define TEXT_X     150
#define SEL_Y      135
#define ITEM_PITCH 56
#define ABOVE_Y    20
#define TEXT_MAX   (HM_W - TEXT_X - 6)
#define DIM_ALPHA  120

#define OPT_X0    84 /* the option list (a category's settings, System's pages) */
#define OPT_TEXT  94
#define OPT_X1    466
#define OPT_PITCH 26

#define POP_W     176 /* a choice's options */
#define POP_PITCH 24
#define MENU_W    208 /* the Triangle menu */
#define SLIDE     120

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

/* A page that is the XMB's option list: rows opened from the section's first page. */
static int is_options(const hm_page *pg, int d)
{
    return d >= 1 && pg->layout == HM_PAGE_ROWS && !pg->over;
}

/* A page for which the categories slide away to the left: an option list, what plays. */
static int is_deep(const hm_page *pg, int d)
{
    return is_options(pg, d) || (d >= 1 && pg->layout == HM_PAGE_NOW);
}

/* How far the bar has slid (0-256), following depth_pos: from the first page of the stack
   (shown, or sliding out) that wants it. */
static int deep(const hm_view *v)
{
    int top = (v->depth_pos + 255) / 256;

    for (int d = 1; d < HM_DEPTH && d <= top; d++)
        if (is_deep(&v->page[d], d))
            return hm_clampi(v->depth_pos - (d - 1) * 256, 0, 256);
    return 0;
}

/* ---- the top right: the clock, the battery ---- */

static void status(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    const pw_image *b = ui->art && ui->art->battery.px ? &ui->art->battery : 0;
    int x1 = 444;

    if (v->battery >= 0 && b && b->h >= 4 && b->h % 4 == 0) {
        /* the XMB's own picture: four frames, full to empty; charging, it fills up in a loop */
        int fh = b->h / 4, frame = v->battery > 66   ? 0
                                   : v->battery > 33 ? 1
                                   : v->battery > 5  ? 2
                                                     : 3;

        if (v->charging)
            frame = 3 - (int)((v->t_ms / 600) % 4);
        g->image(g, b->px + frame * fh * ((b->w + 15) & ~15), b->w, fh, (b->w + 15) & ~15,
                 463 - b->w / 2, 12 - fh / 2, b->w, fh, a);
        x1 = 463 - b->w / 2 - 6;
    } else if (v->battery >= 0) {
        int bars = v->battery > 66 ? 3 : v->battery > 33 ? 2 : v->battery > 5 ? 1 : 0;

        g->rrect(g, 451, 7, 21, 10, 2, PW_FX(1) + 4, C_TEXT, a);
        g->fill(g, 472, 10, 2, 4, C_TEXT, a);
        for (int i = 0; i < bars; i++)
            g->fill(g, 453 + i * 6, 9, 5, 6, v->charging ? PW_RGB(140, 230, 140) : C_TEXT, a);
        x1 = 444;
    }
    if (v->battery >= 0 && v->battery_pct) {
        char pct[8];

        stbsp_snprintf(pct, sizeof(pct), "%d%%", hm_clampi(v->battery, 0, 100));
        stext_right(g, ui->body, x1, 17, pct, C_TEXT, a);
        x1 -= pw_text_width(ui->body, pct) + 10;
    }
    if (v->clock && v->clock[0])
        stext_right(g, ui->body, x1, 17, v->clock, C_TEXT, a);
}

/* ---- the categories ---- */

static void categories(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    const hm_page *root = &v->page[0];
    int dp_open = deep(v), bar_x = CAT_X - CAT_DEEP * dp_open / 256,
        /* the game list's chosen icon reaches up to where the name would be: none there */
        tiles = root->layout == HM_PAGE_GAMES || root->layout == HM_PAGE_ALBUMS ||
                root->layout == HM_PAGE_PHOTOS,
        named = tiles && root->count > 0 ? 0 : 256;

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
                         a * near / 256 * named / 256 * (256 - dp_open) / 256);
    }
}

/* ---- tiles: the XMB's game list ---- */

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

static void folder(hm_gfx *g, const hm_ui *ui, const hm_page *pg, int a)
{
    const hm_tile *sel;
    int near_sel;

    if (pg->count <= 0) {
        if (pg->empty)
            stext(g, ui->title, TEXT_X, SEL_Y + 5, pg->empty, TEXT_MAX, C_TEXT, a);
        return;
    }
    for (int i = 0; i < pg->count; i++) {
        int p = i * 256 - pg->selpos, near = hm_near(p), y = folder_y(p),
            w = hm_lerp(SMALL_W, HM_ICON_W, near), h = hm_lerp(SMALL_H, HM_ICON_H, near);

        if (y + h / 2 < 0 || y - h / 2 > HM_H)
            continue;
        if (pg->tiles[i].icon || pg->layout == HM_PAGE_GAMES)
            hm_tile_draw(g, pg->tiles[i].icon, FOLDER_X - w / 2, y - h / 2, w, h,
                         a * (215 + 41 * near / 256) / 256);
        if (pg->tiles[i].fav)
            hm_fav_star(g, FOLDER_X - w / 2, y - h / 2, w, a);
        else /* a save or an album without a picture: the XMB's folder */
            g->icon(g, HM_ICON_FOLDER, FOLDER_X, y, h * 3 / 4, C_TEXT,
                    a * (215 + 41 * near / 256) / 256);
    }
    sel = &pg->tiles[hm_clampi(pg->sel, 0, pg->count - 1)];
    near_sel = hm_near(pg->sel * 256 - pg->selpos);
    if (near_sel > 128) { /* the chosen one's name, a rule, its line: as the list settles */
        int la = a * (near_sel - 128) / 128;

        stext(g, ui->title, LABEL_X, FOLDER_Y - 8, sel->title, HM_W - 6 - LABEL_X, C_TEXT, la);
        g->fill(g, LABEL_X - 1, FOLDER_Y, HM_W - 6 - LABEL_X, 1, C_TEXT, la * 200 / 256);
        if (sel->meta)
            stext(g, ui->body, LABEL_X, FOLDER_Y + 15, sel->meta, HM_W - 6 - LABEL_X, C_TEXT,
                  la * 220 / 256);
    }
}

/* ---- items with an icon and a line (Music, Play Time, the settings' categories) ---- */

static int row_y(int p) /* the centre of the row p (1/256) from the cursor */
{
    if (p >= 0)
        return SEL_Y + ITEM_PITCH * p / 256;
    if (p >= -256)
        return SEL_Y + (SEL_Y - ABOVE_Y) * p / 256;
    return ABOVE_Y + ITEM_PITCH * (p + 256) / 256;
}

/* An item's picture: a game's icon, the cover playing, the system's item icon, a drawn one. */
static void item_icon(hm_gfx *g, const hm_ui *ui, const hm_view *v, const hm_page *pg, int i, int x,
                      int y, int near, int ia)
{
    const hm_row *r = &pg->rows[i];
    int music = pg->layout == HM_PAGE_MUSIC, icon;

    if (r->icon) { /* a game's ICON0, as the XMB's lists show it */
        int w = hm_lerp(54, 72, near), h = hm_lerp(30, 40, near);

        hm_tile_draw(g, r->icon, x - w / 2, y - h / 2, w, h, ia);
        return;
    }
    if (music && i == 0 && v->mu_cover) { /* what plays: its cover */
        int s = hm_lerp(40, 52, near);

        hm_cover_draw(g, v->mu_cover, x - s / 2, y - s / 2, s, 4, ia);
        return;
    }
    if (r->art && ui->art && ui->art->item[r->art - 1].px) {
        int w = hm_lerp(50, 64, near), h = hm_lerp(40, 52, near);

        hm_art_fit(g, &ui->art->item[r->art - 1], x, y, w, h, ia);
        return;
    }
    icon = r->glyph                           ? r->glyph
           : v->tab == HM_TAB_PLAYTIME        ? PW_ICON_WATCH
           : music && i == 0 && pg->nrows > 1 ? (v->mu_state == 1 ? HM_ICON_PAUSE : HM_ICON_PLAY)
                                              : HM_ICON_MUSIC;
    g->icon(g, icon, x, y, 30 + 8 * near / 256,
            music && i == v->mu_cur + 1 && i > 0 ? PW_RGB(150, 210, 255) : C_TEXT, ia);
}

static void items(hm_gfx *g, const hm_ui *ui, const hm_view *v, const hm_page *pg, int a, int dp)
{
    int shift = -CAT_DEEP * dp / 256;

    for (int i = 0; i < pg->nrows; i++) {
        const hm_row *r = &pg->rows[i];
        int p = i * 256 - pg->row_pos, near = hm_near(p), y = row_y(p),
            ia = a * (DIM_ALPHA + (256 - DIM_ALPHA) * near / 256) / 256,
            ta = ia * hm_clampi(256 - 2 * dp, 0, 256) / 256; /* the words go first */
        const char *line = r->value;

        if (y < -30 || y > HM_H + 30)
            continue;
        if (dp > 0 && i != pg->row_sel) /* only the chosen item's icon stays by the options */
            ia = ia * (256 - dp) / 256;
        if (r->kind == HM_ROW_HEADER || r->kind == HM_ROW_NOTE) {
            stext(g, ui->body, TEXT_X + shift, y + 4, r->label, TEXT_MAX, C_TEXT, ta * 200 / 256);
            continue;
        }
        item_icon(g, ui, v, pg, i, ICON_X + shift, y, near, ia);
        if (pg->layout == HM_PAGE_MUSIC && i == 0 && v->mu_time)
            line = v->mu_time;
        if (line) {
            stext(g, ui->title, TEXT_X + shift, y - 1, r->label, y < 40 ? 290 - TEXT_X : TEXT_MAX,
                  C_TEXT, ta);
            stext(g, ui->body, TEXT_X + shift, y + 15, line, TEXT_MAX, C_TEXT, ta * 210 / 256);
        } else { /* (one near the top: short of the clock) */
            stext(g, ui->title, TEXT_X + shift, y + 5, r->label, y < 40 ? 290 - TEXT_X : TEXT_MAX,
                  C_TEXT, ta);
        }
        if (r->bar >= 0 && near > 0) { /* the most played, the track's time: a thin bar */
            int bw = 160;

            g->fill(g, TEXT_X + shift, y + 21, bw, 2, C_TEXT, ta * 70 / 256);
            g->fill(g, TEXT_X + shift, y + 21, bw * hm_clampi(r->bar, 0, 256) / 256, 2, C_TEXT, ta);
        }
    }
}

/* ---- the XMB's option list ---- */

static void options(hm_gfx *g, const hm_ui *ui, const hm_view *v, const hm_page *pg, int t, int a)
{
    int dp = 256 - (t < 0 ? -t : t), oa = a * dp / 256, slide = t * 40 / 256,
        va = 256 - hm_clampi(v->popup, 0, 256); /* the values give way to a choice's options */

    if (oa <= 0)
        return;
    if (pg->row_sel >= 0 && pg->row_sel < pg->nrows) { /* the focus bar, where the cursor rests */
        g->fill(g, OPT_X0 + slide, SEL_Y - 12, OPT_X1 + 6 - OPT_X0, 24, C_TEXT, oa * 34 / 256);
        g->fill(g, OPT_X0 + slide, SEL_Y - 12, OPT_X1 + 6 - OPT_X0, 1, C_TEXT, oa * 90 / 256);
        g->fill(g, OPT_X0 + slide, SEL_Y + 11, OPT_X1 + 6 - OPT_X0, 1, C_TEXT, oa * 90 / 256);
    }
    for (int i = 0; i < pg->nrows; i++) {
        const hm_row *r = &pg->rows[i];
        int y = SEL_Y + (i * 256 - pg->row_pos) * OPT_PITCH / 256, on = i == pg->row_sel, ra;

        if (y < 30 || y > 252)
            continue;
        ra = oa *
             hm_clampi(y < 56    ? (y - 30) * 256 / 26
                       : y > 226 ? (252 - y) * 256 / 26
                                 : 256,
                       0, 256) /
             256;
        if (r->kind == HM_ROW_HEADER || r->kind == HM_ROW_NOTE) {
            stext(g, ui->body, OPT_TEXT + slide, y + 4, r->label, OPT_X1 - OPT_TEXT, C_TEXT,
                  ra * 170 / 256);
            continue;
        }
        stext(g, ui->title, OPT_TEXT + slide, y + 5, r->label, r->value ? 220 : OPT_X1 - OPT_TEXT,
              C_TEXT, ra * (on ? 256 : 190) / 256);
        if (r->value && va > 0) {
            int x1 = OPT_X1 + slide - (r->kind == HM_ROW_LINK ? 12 : 0);

            stext_right(g, ui->body, x1, y + 4, r->value, C_TEXT,
                        ra * (on ? 256 : 170) / 256 * va / 256);
            if (r->swatch)
                g->rrect(g, x1 - pw_text_width(ui->body, r->value) - 14, y - 4, 9, 9, 2, 0,
                         r->swatch, ra * va / 256);
        }
        if ((r->kind == HM_ROW_ACTION || r->kind == HM_ROW_LINK) && (on || r->kind == HM_ROW_LINK))
            hm_chevron(g, OPT_X1 - 4 + slide, y, 1, C_TEXT, ra * (on ? 256 : 150) / 256);
    }
}

static void hints(hm_gfx *g, const hm_ui *ui, const char *enter, const char *back,
                  const char *options, const char *square, const hm_view *dpad, int a);

/* A choice's options: the XMB's panel sliding in from the right. */
static void popup(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int p = hm_clampi(v->popup, 0, 256), x0 = HM_W - POP_W * p / 256;

    g->fill(g, x0, 0, POP_W, HM_H, C_BLACK, p * 205 / 256);
    g->fill(g, x0, 0, 1, HM_H, C_TEXT, p * 70 / 256);
    g->fill(g, x0 + 8, SEL_Y - 12, POP_W - 16, 24, C_TEXT, p * 40 / 256);
    for (int k = 0; k < v->popup_count; k++) {
        int y = SEL_Y + (k * 256 - v->popup_pos) * POP_PITCH / 256,
            sw = v->popup_swatch[k] ? 16 : 0;

        if (y < 10 || y > HM_H - 10)
            continue;
        if (sw)
            g->rrect(g, x0 + 20, y - 5, 10, 10, 2, 0, v->popup_swatch[k],
                     p * (k == v->popup_sel ? 256 : 160) / 256);
        stext(g, ui->title, x0 + 20 + sw, y + 5, v->popup_items[k], POP_W - 30 - sw, C_TEXT,
              p * (k == v->popup_sel ? 256 : 160) / 256);
    }
    hints(g, ui, v->hint_enter, v->hint_back, 0, 0, 0, p);
}

/* A game's Triangle menu: the XMB's option panel on the right, its rows centred on the cursor. */
static void menu(hm_gfx *g, const hm_ui *ui, const hm_page *pg, int t)
{
    int p = 256 - (t < 0 ? -t : t), x0 = HM_W - MENU_W * p / 256;

    if (p <= 0)
        return;
    g->fill(g, x0, 0, MENU_W, HM_H, C_BLACK, p * 210 / 256);
    g->fill(g, x0, 0, 1, HM_H, C_TEXT, p * 70 / 256);
    if (pg->title)
        stext(g, ui->body, x0 + 18, 30, pg->title, MENU_W - 30, C_TEXT, p * 180 / 256);
    g->fill(g, x0 + 8, SEL_Y - 12, MENU_W - 16, 24, C_TEXT, p * 40 / 256);
    for (int k = 0; k < pg->nrows; k++) {
        const hm_row *r = &pg->rows[k];
        int y = SEL_Y + (k * 256 - pg->row_pos) * POP_PITCH / 256,
            ra = p * (k == pg->row_sel ? 256 : 160) / 256;

        if (y < 44 || y > HM_H - 26 || r->kind == HM_ROW_HEADER)
            continue;
        stext(g, ui->title, x0 + 20, y + 5, r->label, MENU_W - 40 - (r->value ? 30 : 0), C_TEXT,
              ra);
        if (r->value)
            stext_right(g, ui->body, x0 + MENU_W - 16, y + 4, r->value, C_TEXT, ra * 200 / 256);
    }
}

/* What plays, whole: the cover where the item column is, the words beside it. */
static void now(hm_gfx *g, const hm_ui *ui, const hm_view *v, const hm_page *pg, int a)
{
    int cx = 34, cy = 102, size = 108, x = 166, w = HM_W - 20 - x;

    hm_cover_draw(g, v->mu_cover, cx, cy, size, 4, a);
    if (!v->mu_title) {
        stext(g, ui->title, x, 140, pg->empty, w, C_TEXT, a);
        return;
    }
    stext(g, ui->title, x, 104, v->mu_title, w, C_TEXT, a);
    if (v->mu_artist)
        stext(g, ui->body, x, 122, v->mu_artist, w, C_TEXT, a * 220 / 256);
    if (v->mu_album)
        stext(g, ui->body, x, 138, v->mu_album, w, C_TEXT, a * 170 / 256);
    g->fill(g, x, 156, w, 2, C_TEXT, a * 70 / 256);
    if (v->mu_bar >= 0)
        g->fill(g, x, 156, w * hm_clampi(v->mu_bar, 0, 256) / 256, 2, C_TEXT, a);
    if (v->mu_time)
        stext(g, ui->body, x, 174, v->mu_time, 0, C_TEXT, a * 220 / 256);
    stext_right(g, ui->body, x + w, 174, v->mu_length, C_TEXT, a * 220 / 256);
    g->icon(g, HM_ICON_PREV, x + w / 2 - 44, 196, 14, C_TEXT,
            a * (v->mu_seek < 0 ? 256 : 200) / 256);
    g->icon(g, v->mu_state == 1 ? HM_ICON_PAUSE : HM_ICON_PLAY, x + w / 2, 196, 18, C_TEXT, a);
    g->icon(g, HM_ICON_NEXT, x + w / 2 + 44, 196, 14, C_TEXT,
            a * (v->mu_seek > 0 ? 256 : 200) / 256);
    g->icon(g, HM_ICON_SHUFFLE, x + 8, 196, 14, C_TEXT, a * (v->mu_shuffle ? 256 : 90) / 256);
    g->icon(g, v->mu_repeat == 2 ? HM_ICON_REPEAT1 : HM_ICON_REPEAT, x + w - 8, 196, 14, C_TEXT,
            a * (v->mu_repeat ? 256 : 90) / 256);
}

/* ---- the hints: centred at the bottom, the system's button pictures ---- */

static void hints(hm_gfx *g, const hm_ui *ui, const char *enter, const char *back,
                  const char *options, const char *square, const hm_view *dpad, int a)
{
    const char *label[6] = {
        enter, options, square, back, dpad ? dpad->hint_lr : 0, dpad ? dpad->hint_ud : 0};
    int btn[4] = {ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, PW_BTN_TRIANGLE, PW_BTN_SQUARE,
                  ui->cross_enter ? PW_BTN_CIRCLE : PW_BTN_CROSS},
        w = 0, x;

    for (int i = 0; i < 6; i++)
        if (label[i])
            w += 14 + pw_text_width(ui->body, label[i]) + 16;
    x = HM_W / 2 - (w - 16) / 2;
    for (int i = 0; i < 6; i++) {
        if (!label[i])
            continue;
        if (i >= 4)
            g->icon(g, i == 4 ? HM_ICON_LR : HM_ICON_UD, x + 6, 256, 12, C_TEXT, a);
        else
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
            int y = top + 34 + i * rh, base = y + rh / 2 + 4,
                vw = v->panel_values[i] ? pw_text_width(ui->body, v->panel_values[i]) + 14 : 0;

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
        hints(g, ui, v->panel_hint ? v->panel_hint : 0, 0, 0, 0, 0, p);
    }
}

static void sheet(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int p = hm_clampi(v->sheet, 0, 256);

    g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, p * 170 / 256);
    g->fill(g, 60, 92, HM_W - 120, 1, C_TEXT, p);
    g->fill(g, 60, 183, HM_W - 120, 1, C_TEXT, p);
    stext_center(g, ui->title, HM_W / 2, 122, v->sheet_title, HM_W - 140, C_TEXT, p);
    hm_text_wrap(g, ui->body, HM_W / 2, 142, 14, 2, v->sheet_text, HM_W - 140, 1, C_TEXT, p);
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
    hints(g, ui, v->sheet_again, v->sheet_hint, 0, 0, 0, p);
}

/* ---- the screen ---- */

void hm_draw_classic(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int a = hm_clampi(v->appear, 0, 256), dp = deep(v);

    if (v->viewer < 256) {
        if (v->wave > 0)
            g->wave(g, v->t_ms, v->wave * a / 256);
        categories(g, ui, v, a);
        for (int d = 0; d < HM_DEPTH; d++) {
            const hm_page *pg = &v->page[d];
            int t = hm_page_t(v, d);
            hm_shift s;
            hm_gfx *gg;

            if (pg->over)
                continue; /* (drawn over the rest below) */
            if (d == 0 && dp > 0 && pg->layout != HM_PAGE_GAMES &&
                v->page[1].layout != HM_PAGE_NOW) {
                items(g, ui, v, pg, a, dp); /* the items stay, slid left, by the option list */
                continue;
            }
            if (t <= -256 || t >= 256)
                continue;
            if (is_options(pg, d)) {
                options(g, ui, v, pg, t, a);
                continue;
            }
            gg = t ? hm_shift_init(&s, g, t * SLIDE / 256, 256 - (t < 0 ? -t : t)) : g;
            if (pg->layout == HM_PAGE_ROWS || pg->layout == HM_PAGE_MUSIC)
                items(gg, ui, v, pg, a, 0);
            else if (pg->layout == HM_PAGE_NOW)
                now(gg, ui, v, pg, a);
            else
                folder(gg, ui, pg, a);
        }
        status(g, ui, v, a);
        for (int d = 1; d < HM_DEPTH; d++) {
            int t = hm_page_t(v, d);

            if (v->page[d].over && t > -256 && t < 256)
                menu(g, ui, &v->page[d], t);
        }
        if (v->popup <= 0 && v->sheet <= 0 && v->panel <= 0 && hm_top(v)->layout != HM_PAGE_GAMES &&
            hm_top(v)->layout != HM_PAGE_ALBUMS)
            hints(g, ui, v->depth > 0 || hm_top(v)->layout == HM_PAGE_NOW ? v->hint_enter : 0,
                  v->depth > 0 ? v->hint_back : 0, v->depth > 0 ? v->hint_options : 0,
                  v->depth > 0 ? v->hint_square : 0, v, a);
    }
    if (v->popup > 0)
        popup(g, ui, v);
    if (v->panel > 0)
        panel(g, ui, v);
    hm_draw_cal(g, ui, v);
    if (v->sheet > 0)
        sheet(g, ui, v);
    hm_draw_toast(g, ui, v, C_BLACK, C_TEXT);
    hm_draw_keyboard(g, ui, v, 0);
}

void hm_prefetch_classic(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    const hm_page *top = hm_top(v);

    for (int d = 1; d <= 3; d++) /* the games next to the chosen one */
        for (int s = -1; s <= 1; s += 2) {
            int i = top->sel + s * d;

            if (i < 0 || i >= top->count)
                continue;
            if (!g->warm(g, ui->title, top->tiles[i].title ? top->tiles[i].title : "",
                         HM_W - 6 - LABEL_X) ||
                (top->tiles[i].meta &&
                 !g->warm(g, ui->body, top->tiles[i].meta, HM_W - 6 - LABEL_X)))
                return;
        }
    for (int k = 0; k < HM_TABS; k++) { /* the sections' rows near where they will show */
        int here = k == v->tab && top->rows, opt = here && is_options(top, v->depth);
        const hm_row *rows = here ? top->rows : v->other_rows[k];
        int n = here ? top->nrows : v->other_nrows[k], cur = here ? top->row_sel : 0,
            from = cur - 6, to = cur + 8;

        for (int i = from < 0 ? 0 : from; rows && i < n && i <= to; i++) {
            const hm_row *r = &rows[i];
            int head = r->kind == HM_ROW_HEADER || r->kind == HM_ROW_NOTE;
            pw_font *f = head ? ui->body : ui->title;
            int w = opt ? (head       ? OPT_X1 - OPT_TEXT
                           : r->value ? 220
                                      : OPT_X1 - OPT_TEXT)
                        : TEXT_MAX;

            if (!g->warm(g, f, r->label ? r->label : "", w) ||
                (r->value && !g->warm(g, ui->body, r->value, opt ? 0 : TEXT_MAX)))
                return;
        }
    }
}
