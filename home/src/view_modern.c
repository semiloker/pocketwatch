// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Modern (docs/ui/styles.md): the sections as pills at the top left - an icon each, the chosen
 * one opening into a pill with its name; Games as a row of icons over the chosen game's blurred
 * art; lists whose chosen row is a pill gliding under the text, each category with its icon in
 * a tinted square; pages that slide in from the right with their name at the top (ADR-035);
 * albums and photos as grids; Music as a card of what plays (its cover, the time, the controls)
 * beside the tracks, and what plays whole. The colours come from the theme (hm_palette).
 */
#include <string.h>

#include "stb_sprintf.h"
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

#define LIST_X0   24 /* the lists */
#define LIST_X1   456
#define LIST_Y0   44
#define LIST_Y1   248
#define PITCH     30 /* a row; 38 with pictures (Play Time) */
#define PITCH_PIC 38

#define PG_X     26 /* Photos: the grid */
#define PG_W     100
#define PG_H     56
#define PG_GAP   9
#define PG_PITCH 66

#define AL_W     100 /* Albums: cards, two rows of four (136 x 76 left one row on screen and */
#define AL_H     56  /* the chosen one under the edge, 2026-10-20) */
#define AL_GAP   10
#define AL_PITCH 96

#define MU_X   24 /* Music: what plays, on a card */
#define MU_W   168
#define MU_ART 96

#define SLIDE 150 /* how far a page slides in or out (px) */

/* A row of two lines (the tracks, saved data): the value's baseline under the label's - the two
   had 2-3 px between them (2026-10-27) - and the label's so that the pair is centred in the row's
   pill, by the fonts' capitals. */
static int stack_gap(const hm_ui *ui)
{
    return pw_font_cap(ui->body) + 5;
}

static int stack_ty(const hm_ui *ui, int y, int pitch)
{
    return y + pitch / 2 + (pw_font_cap(ui->title) - stack_gap(ui)) / 2;
}

typedef struct {
    int x0, x1, y0, y1, pitch;
    int stack; /* the value under the label (a narrow list: the tracks) */
} box;

/* ---- the top: section pills, the clock and battery ---- */

static int status(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_view *v, int a)
{
    char pct[8] = "";
    int tw = v->clock && v->clock[0] ? pw_text_width(ui->body, v->clock) : 0, pw = 0, w, x;

    if (v->battery >= 0 && v->battery_pct) {
        stbsp_snprintf(pct, sizeof(pct), "%d%%", hm_clampi(v->battery, 0, 100));
        pw = pw_text_width(ui->body, pct) + 6;
    }
    w = tw + (v->battery >= 0 ? 34 + pw : 0) + 20;
    x = HM_W - 12 - w;
    if (w <= 20)
        return HM_W;
    g->rrect(g, x, 9, w, 24, 12, 0, p->card, a * hm_clampi(p->card_a + 20, 0, 256) / 256);
    if (tw)
        g->text(g, ui->body, x + 10, hm_mid(ui->body, 21), v->clock, 0, p->text, a);
    if (v->battery >= 0) {
        int bx = x + w - 30, fill = 14 * hm_clampi(v->battery, 0, 100) / 100;
        unsigned int c = v->charging        ? PW_RGB(110, 220, 120)
                         : v->battery <= 15 ? PW_RGB(240, 90, 80)
                                            : p->text;

        if (pw)
            g->text(g, ui->body, bx - pw, hm_mid(ui->body, 21), pct, 0, v->charging ? c : p->text,
                    a);
        g->rrect(g, bx, 16, 18, 10, 2, PW_FX(1) + 4, p->text, a);
        g->fill(g, bx + 18, 19, 2, 4, p->text, a);
        g->fill(g, bx + 2, 18, fill, 6, c, a);
    }
    return x;
}

/* Each section an icon; the chosen one (following tab_pos, so it glides) opens into a pill that
   carries its name. Too little room: the names in the smaller font. */
static void pills(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_view *v, int a,
                  int limit)
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
            g->rrect(g, x, 9, w, 24, 12, 0, p->sel, a * open / 256 * p->sel_a / 256);
        else
            g->rrect(g, x, 9, w, 24, 12, 0, p->card, a * p->card_a * 3 / 5 / 256);
        g->icon(g, hm_icon_of_tab(i), x + 14, 21, 16, hm_mix(p->text, p->ink, open),
                a * (190 + 66 * open / 256) / 256);
        if (open > 64 && v->tab_names[i])
            g->text(g, f, x + 27, hm_mid(f, 21), v->tab_names[i], 0, p->ink, a * (open - 64) / 192);
        x += w + 4;
    }
}

/* A page's name at the top of its content (a page opened from another): an arrow back and the
   name; a game's page shows its picture and line. Returns where the content starts. */
/* The performance pill (home_view.c) sits on a second row at the top left; when it is on, the
   content below starts this much lower so it is not covered (owner 2026-10-23). */
static int g_perf_pad;

static int header(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_page *pg, int a)
{
    int d = g_perf_pad;

    if (!pg->title)
        return LIST_Y0 + d;
    if (pg->icon) {
        hm_tile_draw(g, pg->icon, LIST_X0, 44 + d, 96, 53, a);
        g->text(g, ui->big, LIST_X0 + 110, 66 + d, pg->title, LIST_X1 - LIST_X0 - 110, p->text, a);
        if (pg->sub)
            g->text(g, ui->body, LIST_X0 + 110, 86 + d, pg->sub, LIST_X1 - LIST_X0 - 110, p->sub,
                    a);
        return 106 + d;
    }
    hm_chevron(g, LIST_X0 + 6, 56 + d, -1, p->sub, a);
    g->text(g, ui->title, LIST_X0 + 16, 61 + d, pg->title, LIST_X1 - LIST_X0 - 16, p->text, a);
    if (pg->sub) { /* the name first: the line in what is left */
        int room = LIST_X1 - LIST_X0 - 16 - pw_text_width(ui->title, pg->title) - 20,
            sw = pw_text_width(ui->body, pg->sub);

        if (room > 60)
            g->text(g, ui->body, LIST_X1 - (sw < room ? sw : room), 61 + d, pg->sub, room, p->sub,
                    a);
    }
    g->fill(g, LIST_X0, 68 + d, LIST_X1 - LIST_X0, 1, p->text, a * 36 / 256);
    return 72 + d;
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

static void button_pill(hm_gfx *g, const hm_ui *ui, const hm_palette *p, int x, int y,
                        const char *label, int a)
{
    int w = pw_text_width(ui->title, label) + 48;

    g->rrect(g, x, y, w, 30, 15, 0, p->sel, a * p->sel_a / 256);
    g->button(g, ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, x + 18, y + 15, 16, p->ink, a);
    g->text(g, ui->title, x + 32, hm_mid(ui->title, y + 15), label, 0, p->ink, a);
}

static void empty_text(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_page *pg, int y,
                       int a)
{
    if (pg->empty)
        hm_text_center(g, ui->title, HM_W / 2, y, pg->empty, HM_W - 40, p->sub, a);
}

/* A game with no ICON0 gets a coloured card with its initial, a deterministic hue per title, so
   the row reads as colourful cards on the dark sky (ADR-050). Games with real art keep it. */
static void game_card(hm_gfx *g, const hm_ui *ui, const hm_tile *t, int x, int y, int w, int h,
                      int a)
{
    static const unsigned int pal[] = {
        PW_RGB(90, 104, 220),  PW_RGB(40, 160, 160), PW_RGB(150, 92, 206), PW_RGB(206, 86, 120),
        PW_RGB(210, 150, 60),  PW_RGB(72, 170, 110), PW_RGB(60, 150, 205), PW_RGB(205, 110, 70),
        PW_RGB(120, 120, 210), PW_RGB(190, 90, 170),
    };
    unsigned int hsh = 2166136261u;
    const char *s = t->title;
    int r = h > 40 ? 10 : 6;
    char ini[2];

    for (const char *q = s; q && *q; q++)
        hsh = (hsh ^ (unsigned char)*q) * 16777619u;
    g->rrect(g, x, y, w, h, r, 0, pal[hsh % (sizeof(pal) / sizeof(pal[0]))], a);
    g->rrect(g, x, y, w, h / 2, r, 0, C_TEXT, a * 22 / 256); /* lit from above: a card */
    while (s && *s == ' ')
        s++;
    ini[0] = s && *s ? (*s >= 'a' && *s <= 'z' ? (char)(*s - 32) : *s) : '?';
    ini[1] = 0;
    hm_text_center(g, ui->big, x + w / 2, hm_mid(ui->big, y + h / 2), ini, w, C_TEXT, a);
}

static void games(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_view *v,
                  const hm_page *pg, int a)
{
    const hm_tile *sel;

    if (pg->count <= 0) {
        empty_text(g, ui, p, pg, 140, a);
        return;
    }
    for (int i = 0; i < pg->count; i++) {
        int t = i * 256 - pg->selpos, x, w, h, ta, tile_a;

        if (t <= -256)
            continue;
        slot(t, &x, &w, &h, &ta);
        if (x >= HM_W)
            break;
        ta = ta * a / 256;
        tile_a = ta * (t > 128 ? 200 : 256) / 256;
        if (pg->tiles[i].icon)
            hm_tile_draw(g, pg->tiles[i].icon, x, ROW_TOP, w, h, tile_a);
        else
            game_card(g, ui, &pg->tiles[i], x, ROW_TOP, w, h, tile_a);
        if (pg->tiles[i].fav)
            hm_fav_star(g, x, ROW_TOP, w, ta);
        if (t > -128 && t < 128) /* the focus ring, fading as the row moves */
            g->rrect(g, x - 4, ROW_TOP - 4, w + 8, h + 8, 9, PW_FX(2), p->text,
                     ta * (128 - (t < 0 ? -t : t)) / 128);
    }
    sel = &pg->tiles[hm_clampi(pg->sel, 0, pg->count - 1)];
    g->text(g, ui->big, SEL_X, TITLE_Y, sel->title ? sel->title : "", HM_W - 2 * SEL_X, p->text, a);
    if (sel->meta)
        g->text(g, ui->body, SEL_X, META_Y, sel->meta, HM_W - 2 * SEL_X, p->sub, a);
    if (v->play_label)
        button_pill(g, ui, p, SEL_X, PLAY_Y, v->play_label, a);
}

/* The Modern landing launcher (ADR-050): a Resume hero for the most recent game, a status card
   (clock, battery) beside it, a row of section tiles below (Games, Music, Photos, Settings,
   System). Focus is pg->opt_sel: 0 the hero, 1..5 the tiles. Classic and Minimal keep the plain
   carousel/list. */
static void dash(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_view *v,
                 const hm_page *pg, int a)
{
    static const int sglyph[5] = {HM_ICON_GAMES, HM_ICON_MUSIC, HM_ICON_PHOTOS, PW_ICON_GEAR,
                                  HM_ICON_SYSTEM};
    static const int stab[5] = {HM_TAB_GAMES, HM_TAB_MUSIC, HM_TAB_PHOTOS, HM_TAB_SETTINGS,
                                HM_TAB_SYSTEM};
    int top = 42 + g_perf_pad, hh = 112, hx = 14, hw = 300, sx = hx + 300 + 8,
        sw = HM_W - (hx + 300 + 8) - 14, focus = hm_clampi(pg->opt_sel, 0, 5);
    unsigned int acc = ui->accent ? ui->accent : C_ON;
    const hm_tile *t = pg->count > 0 ? &pg->tiles[hm_clampi(pg->hero, 0, pg->count - 1)] : 0;

    /* the Resume hero: the most recent game's art, its title, line and Play */
    g->rrect(g, hx, top, hw, hh, 14, 0, p->card, a * p->card_a / 256);
    if (focus == 0)
        g->rrect(g, hx - 3, top - 3, hw + 6, hh + 6, 11, PW_FX(2), p->text, a);
    if (t) {
        if (t->icon)
            hm_tile_draw(g, t->icon, hx + 12, top + 26, 112, 62, a);
        else
            game_card(g, ui, t, hx + 12, top + 26, 112, 62, a);
        if (!t->title || pw_text_width(ui->big, t->title) <= hw - 150) {
            g->text(g, ui->big, hx + 136, top + 40, t->title ? t->title : "", hw - 150, p->text, a);
            if (t->meta)
                g->text(g, ui->body, hx + 136, top + 60, t->meta, hw - 150, p->sub, a);
        } else { /* a long name on two lines (it was cut to "Castleva...", 2026-10-27) */
            hm_text_wrap(g, ui->title, hx + 136, top + 30, 17, 2, t->title, hw - 150, 0, p->text,
                         a);
            if (t->meta)
                g->text(g, ui->body, hx + 136, top + 68, t->meta, hw - 150, p->sub, a);
        }
        if (v->play_label)
            button_pill(g, ui, p, hx + 136, top + 80, v->play_label, a);
    } else {
        hm_text_center(g, ui->title, hx + hw / 2, top + hh / 2, pg->empty ? pg->empty : "", hw - 20,
                       p->sub, a);
    }

    /* the status card: the clock, the battery */
    g->rrect(g, sx, top, sw, hh, 14, 0, p->card, a * p->card_a / 256);
    if (v->clock && v->clock[0])
        hm_text_center(g, ui->title, sx + sw / 2, top + 40, v->clock, sw - 16, p->text, a);
    if (v->battery >= 0) {
        int bx = sx + 16, bw = sw - 32, by = top + 70;
        unsigned int c = v->charging        ? PW_RGB(110, 220, 120)
                         : v->battery <= 15 ? PW_RGB(240, 90, 80)
                                            : acc;
        char pct[12];

        stbsp_snprintf(pct, sizeof(pct), "%d%%%s", hm_clampi(v->battery, 0, 100),
                       v->charging ? " +" : "");
        hm_text_center(g, ui->body, sx + sw / 2, by, pct, sw - 20, c, a);
        g->rrect(g, bx, by + 8, bw, 6, 3, 0, p->knob, a * 200 / 256);
        g->rrect(g, bx, by + 8, bw * hm_clampi(v->battery, 0, 100) / 100, 6, 3, 0, c, a);
    }
    if (v->free_mb >= 0) { /* the card's free space */
        char fs[24];

        if (v->free_mb >= 1024)
            stbsp_snprintf(fs, sizeof(fs), "%d.%d GB free", v->free_mb / 1024,
                           (v->free_mb % 1024) * 10 / 1024);
        else
            stbsp_snprintf(fs, sizeof(fs), "%d MB free", v->free_mb);
        hm_text_center(g, ui->body, sx + sw / 2, top + hh - 12, fs, sw - 12, p->sub, a);
    }

    /* the section tiles: Games, Music, Photos, Settings, System, each its own colour */
    {
        static const unsigned int stint[5] = {PW_RGB(80, 140, 230), PW_RGB(220, 92, 170),
                                              PW_RGB(70, 182, 120), PW_RGB(220, 160, 70),
                                              PW_RGB(80, 190, 210)};
        int gap = 10, tw = (HM_W - 2 * hx - 4 * gap) / 5, y = top + hh + 12,
            th = hm_clampi(HM_H - y - 24, 40, 60);

        for (int i = 0; i < 5; i++) {
            int x = hx + i * (tw + gap), mid = x + tw / 2, on = focus == i + 1;

            g->rrect(g, x, y, tw, th, 10, 0, on ? p->sel : p->card,
                     on ? a * p->sel_a / 256 : a * p->card_a / 256);
            g->rrect(g, x, y, tw, 3, 2, 0, stint[i], a * (on ? 256 : 150) / 256); /* a colour cap */
            if (on)
                g->rrect(g, x - 3, y - 3, tw + 6, th + 6, 11, PW_FX(2), p->text, a);
            g->icon(g, sglyph[i], mid, y + th / 2 - 6, 16, on ? p->ink : stint[i], a);
            hm_text_center(g, ui->body, mid, y + th - 8, v->tab_names[stab[i]], tw - 6,
                           on ? p->ink : p->sub, a);
        }
    }
}

/* ---- grids: the photos of an album, the albums ---- */

/* The top row on screen (1/256 rows) for a grid of cols with show whole rows: the chosen row
   stays second. It follows grid_pos (its own motion, main.c): following selpos, it waited while
   selpos passed the row's other photos and then jumped (the owner's "jerky", 2026-10-20). */
static int grid_first(const hm_page *pg, int cols, int show)
{
    int rows = (pg->count + cols - 1) / cols;

    return hm_clampi(pg->grid_pos - 256, 0, rows > show ? (rows - show) * 256 : 0);
}

/* Rows of a grid fade out at the content's edges. */
static int edge_alpha(int y, int h, int top, int bottom, int a)
{
    a = a * hm_clampi(y < top ? 256 - (top - y) * 256 / 24 : 256, 0, 256) / 256;
    return a * hm_clampi(y + h > bottom ? 256 - (y + h - bottom) * 256 / 24 : 256, 0, 256) / 256;
}

static void photos(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_page *pg, int a)
{
    int sel = hm_clampi(pg->sel, 0, pg->count - 1), y0 = header(g, ui, p, pg, a), first,
        top = y0 + (pg->title ? 6 : 18);
    const hm_tile *t;

    if (pg->count <= 0) {
        empty_text(g, ui, p, pg, 140, a);
        return;
    }
    t = &pg->tiles[sel];
    /* under an album's title two rows fit whole (the third peeks out); the section's own: three */
    first = grid_first(pg, HM_PHOTO_COLS, pg->title ? 2 : 3);
    if (!pg->title) { /* the section's own grid: the chosen one's name above it */
        int mw = t->meta ? pw_text_width(ui->body, t->meta) : 0;

        g->text(g, ui->title, PG_X, 52, t->title ? t->title : "", LIST_X1 - PG_X - mw - 16, p->text,
                a);
        hm_text_right(g, ui->body, LIST_X1, 52, t->meta, p->sub, a);
    }
    for (int i = 0; i < pg->count; i++) {
        int r = i / HM_PHOTO_COLS, col = i % HM_PHOTO_COLS,
            y = top + (r * 256 - first) * PG_PITCH / 256, x = col * (PG_W + PG_GAP) + PG_X, ta;

        if (y + PG_H <= y0 - 4 || y >= 254)
            continue;
        ta = edge_alpha(y, PG_H, top, 250, a);
        g->clip(g, y0 - 4, 251); /* the next row peeks out, cut above the hints */
        hm_tile_draw(g, pg->tiles[i].icon, x, y, PG_W, PG_H, ta * (i == sel ? 256 : 200) / 256);
        if (i == sel)
            g->rrect(g, x - 3, y - 3, PG_W + 6, PG_H + 6, 8, PW_FX(2), p->text, ta);
    }
    g->clip(g, 0, HM_H);
}

static void albums(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_page *pg, int a)
{
    int sel = hm_clampi(pg->sel, 0, pg->count - 1), y0 = header(g, ui, p, pg, a), top = y0 + 10,
        first = grid_first(pg, HM_ALBUM_COLS, 2);

    if (pg->count <= 0) {
        empty_text(g, ui, p, pg, 140, a);
        return;
    }
    for (int i = 0; i < pg->count; i++) {
        int r = i / HM_ALBUM_COLS, col = i % HM_ALBUM_COLS,
            y = top + (r * 256 - first) * AL_PITCH / 256, x = LIST_X0 + col * (AL_W + AL_GAP), ta;
        const hm_tile *t = &pg->tiles[i];

        if (y + AL_H + 34 <= y0 || y >= 252)
            continue;
        ta = edge_alpha(y, AL_H + 34, top, 252, a);
        g->clip(g, y0 - 4, 251);
        /* a stack: two cards behind the cover */
        g->rrect(g, x + 8, y - 6, AL_W - 16, 8, 4, 0, p->card, ta * 120 / 256);
        g->rrect(g, x + 4, y - 3, AL_W - 8, 8, 5, 0, p->card, ta * 170 / 256);
        hm_tile_draw(g, t->icon, x, y, AL_W, AL_H, ta * (i == sel ? 256 : 215) / 256);
        if (i == sel)
            g->rrect(g, x - 3, y - 3, AL_W + 6, AL_H + 6, 9, PW_FX(2), p->text, ta);
        g->text(g, ui->title, x + 2, y + AL_H + 16, t->title ? t->title : "", AL_W - 4,
                i == sel ? p->text : p->dim, ta);
        if (t->meta)
            g->text(g, ui->body, x + 2, y + AL_H + 30, t->meta, AL_W - 4, p->sub, ta);
    }
    g->clip(g, 0, HM_H);
}

/* ---- the lists ---- */

static int has_pictures(const hm_row *rows, int n)
{
    for (int i = 0; i < n; i++)
        if (rows[i].icon)
            return 1;
    return 0;
}

static box box_of(const hm_page *pg, int y0)
{
    box b = {LIST_X0, LIST_X1, y0, LIST_Y1, PITCH, 0};

    if (pg->layout == HM_PAGE_MUSIC) { /* beside the card of what plays: the artist under it */
        b.x0 = MU_X + MU_W + 12;
        b.y1 = LIST_Y1 - 6; /* (its last row fades out above the hints) */
        b.pitch = PITCH_PIC;
        b.stack = 1;
    } else if (pg->layout == HM_PAGE_SAVES) {
        b.pitch = 46;
        b.stack = 1;
    }
    if (has_pictures(pg->rows, pg->nrows) && b.pitch < PITCH_PIC)
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
                   ? pw_text_width(ui->body, r->value) +
                         (r->kind == HM_ROW_CHOICE || r->kind == HM_ROW_LINK ? 30 : 0) +
                         (r->swatch ? 16 : 0) + 14
               : r->kind == HM_ROW_LINK ? 30
                                        : 16;

    *lx = b.x0 + 14;
    if (r->icon)
        *lx += (b.pitch - 10) * 9 / 5 + 10;
    else if (r->glyph)
        *lx += 30;
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

/* A colour shown before a value (an accent, a sky's colour). */
static void swatch(hm_gfx *g, int x1, int mid, unsigned int c, int a)
{
    g->rrect(g, x1 - 10, mid - 5, 10, 10, 5, 0, c, a);
}

static void list(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_row *rows, int n,
                 int sel, int pos, box b, int a, unsigned int t_ms, int playing, int moving)
{
    int show = (b.y1 - b.y0) / b.pitch,
        first = hm_clampi(pos - (show / 2) * 256, 0, n > show ? (n - show) * 256 : 0);
    unsigned int acc = ui->accent ? ui->accent : C_ON;

    g->clip(g, b.y0 - 2, b.y1 + 3); /* nothing over the header's line or the hints */
    if (sel >= 0 && sel < n && rows[sel].kind != HM_ROW_HEADER && rows[sel].kind != HM_ROW_NOTE) {
        int py = b.y0 + (pos - first) * b.pitch / 256, r = (b.pitch - 4) / 2;

        g->rrect(g, b.x0, py + 2, b.x1 - b.x0, b.pitch - 4, r < 14 ? r : 14, 0, p->sel,
                 a * p->sel_a / 256);
    }
    for (int i = 0; i < n; i++) {
        const hm_row *r = &rows[i];
        int y = b.y0 + (i * 256 - first) * b.pitch / 256, mid = y + b.pitch / 2,
            base = hm_mid(ui->title, mid), vbase = hm_mid(ui->body, mid),
            on = i == sel && hm_near(i * 256 - pos) > 128, lx, lw, ra;
        unsigned int ct = on ? p->ink : p->dim, cv = on ? p->ink2 : p->sub;

        if (y <= b.y0 - b.pitch || y >= b.y1)
            continue;
        { /* rows leaving the list fade out by their words: gone before those reach the header's
             line or the hints (a half row's text was drawn over the hints, 2026-10-27) */
            int top = (b.stack ? stack_ty(ui, y, b.pitch) : base) - pw_font_cap(ui->title),
                low = b.stack ? stack_ty(ui, y, b.pitch) + (r->value ? stack_gap(ui) : 0) : base;

            ra = a * hm_clampi((top - b.y0 + 6) * 256 / 6, 0, 256) / 256;
            ra = ra * hm_clampi((b.y1 + 4 - low) * 256 / 6, 0, 256) / 256;
            if (ra <= 0)
                continue;
        }
        label_room(ui, r, b, playing >= 0 && i == playing, &lx, &lw);
        if (r->kind == HM_ROW_HEADER) {
            g->text(g, ui->body, lx, y + b.pitch - 8, r->label ? r->label : "", lw, p->head, ra);
            continue;
        }
        if (r->kind == HM_ROW_NOTE) {
            g->text(g, ui->body, lx, vbase, r->label ? r->label : "", lw, p->sub, ra * 200 / 256);
            continue;
        }
        if (r->icon) {
            int ih = b.pitch - 10, iw = ih * 9 / 5;

            hm_tile_draw(g, r->icon, b.x0 + 10, y + 5, iw, ih, ra);
        } else if (r->glyph) { /* the category's icon in its square */
            g->rrect(g, b.x0 + 10, mid - 11, 22, 22, 6, 0, hm_glyph_tint(r->glyph, acc), ra);
            g->icon(g, r->glyph, b.x0 + 21, mid, 14, C_TEXT, ra);
        }
        if (i == playing)
            bars(g, lx - 14, b.stack ? stack_ty(ui, y, b.pitch) : base, t_ms, moving,
                 on ? acc : p->head, ra);
        if (b.stack) { /* the label, the value under it */
            int ty = stack_ty(ui, y, b.pitch);

            g->text(g, ui->title, lx, ty, r->label ? r->label : "", lw,
                    i == playing && !on ? p->head : ct, ra);
            if (r->value)
                g->text(g, ui->body, lx, ty + stack_gap(ui), r->value, lw, cv, ra);
            if (r->bar >= 0) {
                int bw = b.x1 - 14 - lx;

                g->fill(g, lx, y + b.pitch - 5, bw, 2, on ? hm_mix(p->sel, p->ink, 60) : p->knob,
                        ra);
                g->fill(g, lx, y + b.pitch - 5, bw * hm_clampi(r->bar, 0, 256) / 256, 2, acc, ra);
            }
            continue;
        }
        g->text(g, ui->title, lx, base, r->label ? r->label : "", lw,
                i == playing && !on ? p->head : ct, ra);
        if (r->toggle) {
            hm_switch(g, b.x1 - 14 - 30, mid, r->toggle == 2, on && p->sel == acc ? C_TEXT : acc,
                      p->knob, ra);
        } else if (r->value && r->kind == HM_ROW_CHOICE && on) {
            /* the value between the ways to change it */
            int x1 = b.x1 - 16, tw = pw_text_width(ui->body, r->value);

            hm_text_right(g, ui->body, x1 - 14, vbase, r->value, cv, ra);
            if (r->swatch)
                swatch(g, x1 - 14 - tw - 6, mid, r->swatch, ra);
            hm_chevron(g, x1 - 6, mid, 1, p->ink, ra);
            hm_chevron(g, x1 - 22 - tw - (r->swatch ? 16 : 0), mid, -1, p->ink, ra);
        } else if (r->kind == HM_ROW_LINK) {
            int x1 = b.x1 - 30;

            hm_chevron(g, b.x1 - 18, mid, 1, on ? p->ink : p->sub, ra);
            if (r->value) {
                hm_text_right(g, ui->body, x1, vbase, r->value, cv, ra);
                if (r->swatch)
                    swatch(g, x1 - pw_text_width(ui->body, r->value) - 6, mid, r->swatch, ra);
            }
        } else if (r->value) {
            hm_text_right(g, ui->body, b.x1 - 14, vbase, r->value, cv, ra);
            if (r->swatch)
                swatch(g, b.x1 - 14 - pw_text_width(ui->body, r->value) - 6, mid, r->swatch, ra);
        } else if (r->kind == HM_ROW_ACTION) {
            hm_chevron(g, b.x1 - 18, mid, 1, on ? p->ink : p->sub, ra);
        }
        if (r->bar >= 0) { /* under the label: how much of the most (Play Time), of the track */
            int bw = b.x1 - 14 - lx;

            g->fill(g, lx, y + b.pitch - 8, bw, 2, on ? hm_mix(p->sel, p->ink, 60) : p->knob, ra);
            g->fill(g, lx, y + b.pitch - 8, bw * hm_clampi(r->bar, 0, 256) / 256, 2, acc, ra);
        }
    }
    g->clip(g, 0, HM_H);
    if (n > show) { /* where in the list: a thin bar at the right */
        int h = b.y1 - b.y0, th = hm_clampi(h * show / n, 12, h),
            ty = b.y0 + (h - th) * first / ((n - show) * 256);

        g->rrect(g, b.x1 + 8, b.y0, 3, h, 1, 0, p->text, a * 40 / 256);
        g->rrect(g, b.x1 + 8, ty, 3, th, 1, 0, p->text, a * 170 / 256);
    }
}

/* Saved data, save states: each one's picture, title and line as a row. */
static void saves(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_page *pg, int a)
{
    static hm_row rows[64];
    int y0 = header(g, ui, p, pg, a), n = pg->count < 64 ? pg->count : 64;
    box b;

    if (n <= 0) {
        empty_text(g, ui, p, pg, 150, a);
        return;
    }
    for (int i = 0; i < n; i++) {
        memset(&rows[i], 0, sizeof(rows[i]));
        rows[i].label = pg->tiles[i].title;
        rows[i].value = pg->tiles[i].meta;
        rows[i].icon = pg->tiles[i].icon;
        rows[i].bar = -1;
        rows[i].kind = HM_ROW_ACTION;
        if (!rows[i].icon)
            rows[i].glyph = HM_ICON_FOLDER;
    }
    b = box_of(pg, y0 + 2);
    list(g, ui, p, rows, n, pg->sel, pg->selpos, b, a, 0, -1, 0);
}

/* ---- Music: what plays on a card at the left, the tracks at the right ---- */

/* The time: a track with the knob where the music is; chosen (focus 0-256, Now Playing) the
   track thickens and the knob grows a halo - left / right seek it. */
static void progress(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_view *v, int x,
                     int y, int w, int focus, int a)
{
    unsigned int acc = ui->accent ? ui->accent : C_ON;
    int k = w * hm_clampi(v->mu_bar < 0 ? 0 : v->mu_bar, 0, 256) / 256, th = 4 + 2 * focus / 256,
        ty = y + 2 - th / 2, kr = 5 + 2 * focus / 256;

    g->rrect(g, x, ty, w, th, th / 2, 0, p->knob, a);
    if (v->mu_bar >= 0) {
        g->rrect(g, x, ty, k < th ? th : k, th, th / 2, 0, acc, a);
        if (focus > 0)
            g->rrect(g, x + k - kr - 5, y + 2 - kr - 5, 2 * kr + 10, 2 * kr + 10, kr + 5, 0, C_TEXT,
                     a * focus / 256 * 60 / 256);
        g->rrect(g, x + k - kr, y + 2 - kr, 2 * kr, 2 * kr, kr, 0, C_TEXT, a);
    }
    if (v->mu_time)
        g->text(g, ui->body, x, y + 19, v->mu_time, 0, p->sub, a);
    hm_text_right(g, ui->body, x + w, y + 19, v->mu_length, p->sub, a);
}

static void music(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_view *v,
                  const hm_page *pg, int a)
{
    const char *title = v->mu_title ? v->mu_title : pg->nrows > 1 ? pg->rows[0].label : 0;
    int x = MU_X, y = LIST_Y0, ax = x + (MU_W - MU_ART) / 2, ay = y + 10, ring, ty;
    box b = box_of(pg, LIST_Y0);

    g->rrect(g, x, y, MU_W, LIST_Y1 - y, 16, 0, p->card, a * p->card_a / 256);
    ring = pg->row_sel == 0 ? hm_near(pg->row_pos) : 0;
    if (ring > 0)
        g->rrect(g, x - 3, y - 3, MU_W + 6, LIST_Y1 - y + 6, 18, PW_FX(2), p->text, a * ring / 256);
    hm_cover_draw(g, v->mu_cover, ax, ay, MU_ART, 12, a);
    ty = ay + MU_ART + 18;
    if (title) /* the smaller font when the title is long */
        g->text(g, pw_text_width(ui->title, title) <= MU_W - 24 ? ui->title : ui->body, x + 12, ty,
                title, MU_W - 24, p->text, a);
    if (v->mu_artist && v->mu_artist[0])
        g->text(g, ui->body, x + 12, ty + 14, v->mu_artist, MU_W - 24, p->sub, a);
    if (v->mu_title) { /* the time, and the way to the player (the buttons are there) */
        const char *now = v->mu_now_label;
        int bw = now ? pw_text_width(ui->body, now) + 34 : 0, by = ty + 50;

        progress(g, ui, p, v, x + 12, ty + 23, MU_W - 24, 0, a);
        if (now && bw <= MU_W - 16) {
            g->rrect(g, x + (MU_W - bw) / 2, by, bw, 22, 11, 0, p->text, a * 30 / 256);
            hm_button_pic(g, ui, PW_BTN_TRIANGLE, x + (MU_W - bw) / 2 + 14, by + 11, 12, p->text,
                          a);
            g->text(g, ui->body, x + (MU_W - bw) / 2 + 26, hm_mid(ui->body, by + 11), now, 0,
                    p->text, a);
        }
    }
    if (pg->nrows > 1)
        list(g, ui, p, pg->rows + 1, pg->nrows - 1, pg->row_sel - 1, pg->row_pos - 256, b, a,
             v->t_ms, v->mu_cur, v->mu_state == 1);
    else if (pg->nrows == 1)
        hm_text_center(g, ui->title, (b.x0 + b.x1) / 2, (LIST_Y0 + LIST_Y1) / 2, pg->rows[0].label,
                       b.x1 - b.x0 - 20, p->sub, a);
}

/* What plays, whole: the cover at the left, the words, the time and the controls at the right
   (the cover blurred behind it all). */
static void now(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_view *v,
                const hm_page *pg, int a)
{
    int cx = 30, cy = 56, size = 152, x = 204, w = LIST_X1 - x;
    hm_player_look k = {58,      18, 20,  22,     p->text, p->sub, ui->accent ? ui->accent : C_ON,
                        p->text, 40, 238, p->text};

    g->rrect(g, cx + 4, cy + 6, size, size, 14, 0, C_BLACK, a * 90 / 256); /* its shadow */
    hm_cover_draw(g, v->mu_cover, cx, cy, size, 14, a);
    if (!v->mu_title) {
        g->text(g, ui->title, x, 120, pg->empty ? pg->empty : "", w, p->sub, a);
        return;
    }
    g->text(g, pw_text_width(ui->big, v->mu_title) <= w ? ui->big : ui->title, x, 84, v->mu_title,
            w, p->text, a);
    if (v->mu_artist)
        g->text(g, ui->title, x, 106, v->mu_artist, w, p->dim, a);
    if (v->mu_album)
        g->text(g, ui->body, x, 124, v->mu_album, w, p->sub, a);
    progress(g, ui, p, v, x, 146, w, v->mu_seek ? 256 : 0, a);
    hm_player(g, ui, v, &k, x + w / 2, 198, a);
}

/* The two-pane settings (ADR-050): a sidebar of categories on the left, the chosen category's
   options on the right; the focused pane is highlighted. */
static void draw_settings(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_view *v,
                          const hm_page *pg, int a)
{
    unsigned int acc = ui->accent ? ui->accent : C_ON;
    /* (134 px cut the categories' names to "Music a...", "Sound a...", 2026-10-27) */
    int top = LIST_Y0 + g_perf_pad, sbx = 12, sbw = 158, sbp = 32,
        side = pg->opt_focus_pos < 128, /* the sidebar has the cursor */
        show = (LIST_Y1 - top) / sbp,
        first = hm_clampi(pg->row_pos - (show / 2) * 256, 0,
                          pg->nrows > show ? (pg->nrows - show) * 256 : 0);
    box b = {sbx + sbw + 14, LIST_X1, top, LIST_Y1, PITCH, 0};

    g->clip(g, top - 2, LIST_Y1 + 3); /* the sidebar's rows stop above the hints */
    for (int i = 0; i < pg->nrows; i++) {
        const hm_row *r = &pg->rows[i];
        int y = top + (i * 256 - first) * sbp / 256, mid = y + sbp / 2, on = i == pg->row_sel,
            /* a row half under the hints fades out whole (Network peeked out, 2026-10-27) */
            ra = a * hm_clampi((LIST_Y1 + 4 - hm_mid(ui->body, mid)) * 256 / 6, 0, 256) / 256 *
                 hm_clampi((y + 6 - (top - 2)) * 256 / 10, 0, 256) / 256;

        if (y <= top - sbp || y >= LIST_Y1 || ra <= 0)
            continue;
        if (on)
            g->rrect(g, sbx, y + 2, sbw, sbp - 4, 10, 0, side ? p->sel : p->card,
                     side ? ra * p->sel_a / 256 : ra * p->card_a * 3 / 5 / 256);
        if (r->glyph) {
            g->rrect(g, sbx + 8, mid - 10, 20, 20, 6, 0, hm_glyph_tint(r->glyph, acc), ra);
            g->icon(g, r->glyph, sbx + 18, mid, 13, C_TEXT, ra);
        }
        g->text(g, ui->body, sbx + 34, hm_mid(ui->body, mid), r->label ? r->label : "", sbw - 40,
                on && side ? p->ink : p->text, ra);
    }
    g->clip(g, 0, HM_H);
    if (pg->opt_n > 0) {
        list(g, ui, p, pg->opt, pg->opt_n, side ? -1 : pg->opt_sel, side ? 0 : pg->opt_pos, b, a,
             v->t_ms, -1, 0);
    } else if (pg->row_sel >= 0 && pg->row_sel < pg->nrows) {
        /* Wi-Fi / Calibration: a page of its own, opened from the sidebar */
        const hm_row *r = &pg->rows[pg->row_sel];

        hm_text_center(g, ui->body, (b.x0 + b.x1) / 2, b.y0 + 56, r->label ? r->label : "",
                       b.x1 - b.x0, p->sub, a * 200 / 256);
    }
}

/* ---- one page, wherever it is ---- */

static void page(hm_gfx *g, const hm_ui *ui, const hm_palette *p, const hm_view *v,
                 const hm_page *pg, int a)
{
    switch (pg->layout) {
    case HM_PAGE_GAMES:
        if (pg->is_dash)
            dash(g, ui, p, v, pg, a);
        else
            games(g, ui, p, v, pg, a);
        break;
    case HM_PAGE_PHOTOS:
        photos(g, ui, p, pg, a);
        break;
    case HM_PAGE_ALBUMS:
        albums(g, ui, p, pg, a);
        break;
    case HM_PAGE_SAVES:
        saves(g, ui, p, pg, a);
        break;
    case HM_PAGE_MUSIC:
        music(g, ui, p, v, pg, a);
        break;
    case HM_PAGE_NOW:
        now(g, ui, p, v, pg, a);
        break;
    default:
        if (pg->is_settings) {
            draw_settings(g, ui, p, v, pg, a);
        } else {
            int y0 = header(g, ui, p, pg, a);

            if (pg->nrows <= 0)
                empty_text(g, ui, p, pg, 140, a);
            else
                list(g, ui, p, pg->rows, pg->nrows, pg->row_sel, pg->row_pos, box_of(pg, y0), a,
                     v->t_ms, -1, 0);
        }
        break;
    }
}

/* ---- over everything: the HOME panel, a message card ---- */

static void panel(hm_gfx *g, const hm_ui *ui, const hm_palette *pal, const hm_view *v)
{
    int pics = 0, wide = 0, glyphs = 0;
    /* a see-through theme's cards would vanish over the list: the dark card there */
    int solid = pal->card_a >= 128;
    unsigned int card = solid ? pal->card : C_CARD, off = solid ? pal->dim : C_DIM,
                 sub = solid ? pal->sub : C_SUB, acc = ui->accent ? ui->accent : C_ON;

    for (int i = 0; i < v->panel_count; i++) {
        pics |= v->panel_icons[i] != 0;
        wide |= v->panel_values[i] != 0;
        glyphs |= v->panel_glyph[i] != 0;
    }
    {
        int p = hm_clampi(v->panel, 0, 256), n = v->panel_count, rh = pics ? 44 : PITCH,
            batt = v->battery >= 0 ? 30 : 0,
            w = pics || wide ? 400
                : glyphs     ? 300
                             : 260,
            hint = v->panel_hint ? 22 : 0, h = 40 + n * rh + 8 + batt + hint,
            y0 = (HM_H - h) / 2 + (256 - p) * 12 / 256, x0 = (HM_W - w) / 2,
            tx = x0 + (pics     ? 96
                       : glyphs ? 50
                                : 22);

        /* the page under it darker: its words showed through beside the card (2026-10-27) */
        g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, p * 185 / 256);
        g->rrect(g, x0, y0, w, h, 16, 0, card, p * 248 / 256);
        if (v->panel_title)
            g->text(g, ui->title, x0 + 20, y0 + 26, v->panel_title, w - 40, sub, p);
        g->rrect(g, x0 + 8, y0 + 38 + v->panel_pos * rh / 256, w - 16, rh - 4,
                 (rh - 4) / 2 < 12 ? (rh - 4) / 2 : 12, 0, pal->sel, p * pal->sel_a / 256);
        for (int i = 0; i < n; i++) {
            int y = y0 + 38 + i * rh, mid = y + rh / 2, base = hm_mid(ui->title, mid),
                vbase = hm_mid(ui->body, mid), on = hm_near(i * 256 - v->panel_pos) > 128;

            if (v->panel_icons[i])
                g->image(g, v->panel_icons[i], HM_ICON_W, HM_ICON_H, HM_ICON_W, x0 + 18, y + 4, 64,
                         36, p);
            else if (v->panel_glyph[i]) { /* the action's icon in a tinted square */
                g->rrect(g, x0 + 16, mid - 11, 22, 22, 6, 0, hm_glyph_tint(v->panel_glyph[i], acc),
                         p);
                g->icon(g, v->panel_glyph[i], x0 + 27, mid, 14, C_TEXT, p);
            }
            if (v->panel_items[i]) {
                int lw = v->panel_slider[i] >= 0 ? w * 3 / 5 - (tx - x0) - 8
                         : v->panel_values[i]    ? 150
                                                 : w - (tx - x0) - 22;

                g->text(g, ui->title, tx, base, v->panel_items[i], lw, on ? pal->ink : off, p);
            }
            if (v->panel_slider[i] >= 0) { /* a slider row (brightness): a bar with a knob */
                int sbx = x0 + w * 3 / 5, sbw = x0 + w - 22 - sbx,
                    fill = sbw * hm_clampi(v->panel_slider[i], 0, 256) / 256;
                unsigned int c = on ? pal->ink : acc;

                g->rrect(g, sbx, mid - 2, sbw, 4, 2, 0, pal->knob, p * 200 / 256);
                g->rrect(g, sbx, mid - 2, fill, 4, 2, 0, c, p);
                g->rrect(g, sbx + fill - 3, mid - 5, 6, 10, 3, 0, on ? pal->ink : C_TEXT, p);
            } else if (v->panel_values[i])
                hm_text_right(g, ui->body, x0 + w - 22, vbase, v->panel_values[i],
                              on ? pal->ink2 : sub, p);
        }
        if (v->battery >= 0) { /* a battery bar at the bottom of the card */
            int by = y0 + 38 + n * rh + 2, bw = w - 32;
            unsigned int c = v->charging        ? PW_RGB(110, 220, 120)
                             : v->battery <= 15 ? PW_RGB(240, 90, 80)
                                                : acc;
            char pct[12];

            stbsp_snprintf(pct, sizeof(pct), "%d%%%s", hm_clampi(v->battery, 0, 100),
                           v->charging ? " +" : "");
            g->icon(g, HM_ICON_BATTERY, x0 + 22, by + 8, 12, sub, p);
            hm_text_right(g, ui->body, x0 + w - 16, by + 12, pct, c, p);
            g->rrect(g, x0 + 16, by + 20, bw, 5, 2, 0, pal->knob, p * 200 / 256);
            g->rrect(g, x0 + 16, by + 20, bw * hm_clampi(v->battery, 0, 100) / 100, 5, 2, 0, c, p);
        }
        if (v->panel_hint) {
            int tw = pw_text_width(ui->body, v->panel_hint);

            g->text(g, ui->body, x0 + w - 20 - tw, y0 + h - 12, v->panel_hint, 0, sub, p);
            g->button(g, ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, x0 + w - 32 - tw,
                      y0 + h - 16, 12, sub, p);
        }
    }
}

static void sheet(hm_gfx *g, const hm_ui *ui, const hm_palette *pal, const hm_view *v)
{
    int p = hm_clampi(v->sheet, 0, 256), w = 400, h = 116, x0 = (HM_W - w) / 2,
        y0 = (HM_H - h) / 2 + (256 - p) * 12 / 256, solid = pal->card_a >= 128;
    unsigned int acc = ui->accent ? ui->accent : C_ON, card = solid ? pal->card : C_CARD,
                 text = solid ? pal->text : C_TEXT, sub = solid ? pal->sub : C_SUB,
                 tint = v->sheet_glyph ? hm_glyph_tint(v->sheet_glyph, acc) : acc;

    g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, p * 170 / 256);
    g->rrect(g, x0, y0, w, h, 16, 0, card, p * 245 / 256);
    g->rrect(g, x0 + 16, y0 + 16, 28, 28, 8, 0, tint, p);
    g->icon(g, v->sheet_glyph ? v->sheet_glyph : PW_ICON_CARD, x0 + 30, y0 + 30, 16, C_TEXT, p);
    if (v->sheet_title)
        g->text(g, ui->title, x0 + 54, hm_mid(ui->title, y0 + 30), v->sheet_title, w - 74, text, p);
    hm_text_wrap(g, ui->body, x0 + 20, y0 + 64, 14, 2, v->sheet_text, w - 40, 0, sub, p);
    if (v->sheet_busy)
        for (int i = 0; i < 3; i++) { /* three dots, a wave through them */
            int t = (int)((v->t_ms / 4 + (unsigned int)(i * 60)) % 256),
                k = t < 128 ? t * 2 : (255 - t) * 2;

            g->rrect(g, x0 + w - 56 + i * 12, y0 + 28, 6, 6, 3, 0, acc,
                     p * (90 + k * 166 / 256) / 256);
        }
    if (v->sheet_hint) {
        int tw = pw_text_width(ui->body, v->sheet_hint), x = x0 + w - 20 - tw;

        g->text(g, ui->body, x, y0 + h - 14, v->sheet_hint, 0, sub, p);
        g->button(g, ui->cross_enter ? PW_BTN_CIRCLE : PW_BTN_CROSS, x - 12, y0 + h - 18, 12, sub,
                  p);
        if (v->sheet_again) { /* Enter, to the left of the way out */
            x -= 30 + pw_text_width(ui->body, v->sheet_again);
            g->text(g, ui->body, x, y0 + h - 14, v->sheet_again, 0, text, p);
            g->button(g, ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, x - 12, y0 + h - 18, 12,
                      text, p);
        }
    }
}

/* ---- the screen ---- */

void hm_draw_modern(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int a = hm_clampi(v->appear, 0, 256);
    hm_palette p;

    g_perf_pad = v->perf_fps ? 22 : 0; /* room for the performance pill's row */
    hm_palette_of(ui, &p);
    if (v->viewer < 256) {
        if (p.shade || v->dim)
            g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, hm_clampi(p.shade + v->dim, 0, 256));
        pills(g, ui, &p, v, a, status(g, ui, &p, v, a) - 12);
        /* the pages: the one shown, and while the stack moves the one before or after it */
        for (int d = 0; d < HM_DEPTH; d++) {
            int t = hm_page_t(v, d);
            hm_shift s;

            if (t <= -256 || t >= 256)
                continue;
            /* the leaving page fades faster than it moves: no muddle of the two */
            page(t ? hm_shift_init(&s, g, t * SLIDE / 256,
                                   (256 - (t < 0 ? -t : t)) * (256 - (t < 0 ? -t : t)) / 256)
                   : g,
                 ui, &p, v, &v->page[d], a);
        }
        hm_hints_right(g, ui, v, HM_W - 14, 264, p.sub, a);
    }
    if (v->panel > 0)
        panel(g, ui, &p, v);
    hm_draw_cal(g, ui, v);
    if (v->sheet > 0)
        sheet(g, ui, &p, v);
    hm_draw_toast(g, ui, v, p.card_a >= 128 ? p.card : C_CARD, p.card_a >= 128 ? p.text : C_TEXT);
    hm_draw_keyboard(g, ui, v, &p);
}

void hm_prefetch_modern(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    const hm_page *top = hm_top(v);

    /* the games next to the chosen one, nearest first, as games() draws them */
    for (int d = 1; d <= 3 && top->layout == HM_PAGE_GAMES; d++)
        for (int s = -1; s <= 1; s += 2) {
            int i = top->sel + s * d;
            const hm_tile *t;

            if (i < 0 || i >= top->count)
                continue;
            t = &top->tiles[i];
            if (!g->warm(g, ui->big, t->title ? t->title : "", HM_W - 2 * SEL_X) ||
                (t->meta && !g->warm(g, ui->body, t->meta, HM_W - 2 * SEL_X)))
                return; /* out of time this frame */
        }
    /* the sections' rows, as list() draws them: around the cursor in this one, the first ones of
       the others (a long list - the music - is not made all at once) */
    for (int k = 0; k < HM_TABS; k++) {
        int here = k == v->tab && top->rows;
        const hm_row *rows = here ? top->rows : v->other_rows[k];
        int n = here ? top->nrows : v->other_nrows[k], cur = here ? top->row_sel : 0,
            from = cur - 8, to = cur + 8;
        hm_page pg;
        box b;

        if (!rows)
            continue;
        memset(&pg, 0, sizeof(pg));
        pg.layout = here ? top->layout : k == HM_TAB_MUSIC ? HM_PAGE_MUSIC : HM_PAGE_ROWS;
        if (pg.layout == HM_PAGE_MUSIC) { /* its list starts at the second row */
            rows++;
            n--;
            from--;
            to--;
        }
        pg.rows = rows;
        pg.nrows = n;
        b = box_of(&pg, here && top->title ? 72 : LIST_Y0);
        for (int i = from < 0 ? 0 : from; i < n && i <= to; i++) {
            const hm_row *r = &rows[i];
            int lx, lw;

            label_room(ui, r, b, pg.layout == HM_PAGE_MUSIC && i == v->mu_cur, &lx, &lw);
            if (!g->warm(g,
                         r->kind == HM_ROW_HEADER || r->kind == HM_ROW_NOTE ? ui->body : ui->title,
                         r->label ? r->label : "", lw) ||
                (r->value && !g->warm(g, ui->body, r->value, b.stack ? lw : 0)))
                return;
        }
    }
    for (int i = 0; i < v->panel_count; i++)
        if (v->panel_items[i] && !g->warm(g, ui->title, v->panel_items[i], 0))
            return;
}
