// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdlib.h>
#include <string.h>

#include "hcfg.h"
#include "hm_icons.h"
#include "hsettings.h"
#include "hw.h"
#include "library.h"
#include "music.h"
#include "pages.h"
#include "stb_sprintf.h"
#include "ui/i18n.h"
#include "ui/icons.h"

#define ROWS 40
#define VAL  64

/* Every page's rows, values and actions: a page is at most once in the stack. */
typedef struct {
    hm_row r[ROWS];
    char val[ROWS][VAL];
    hm_row_act act[ROWS];
    char title[96], sub[96];
} rowbuf;

static rowbuf g_rows[P_COUNT];
static hm_row g_mu_rows[HM_TRACKS_MAX + 1];
static hm_row_act g_mu_act[HM_TRACKS_MAX + 1];
static char g_mu_now[32], g_mu_len[16];
static hm_row g_pt_rows[HM_GAMES_MAX + 2];
static hm_row_act g_pt_act[HM_GAMES_MAX + 2];
static char g_pt_vals[HM_GAMES_MAX + 2][48];

static hm_tile g_game_tiles[HM_GAMES_MAX];
static hm_tile g_album_tiles[HM_ALBUMS_MAX];
static char g_album_meta[HM_ALBUMS_MAX][32];
static hm_tile g_photo_tiles[HM_PHOTOS_MAX];
static int g_album_list[HM_PHOTOS_MAX], g_album_n, g_album_of = -1;
static hm_state g_states[HM_STATES_MAX];
static int g_nstates, g_states_of = -1;
static hm_tile g_state_tiles[HM_STATES_MAX];
static char g_state_names[HM_STATES_MAX][40];
static hm_save g_saves[HM_SAVES_MAX];
static hm_pic_job g_save_pics[HM_SAVES_MAX];
static int g_nsaves, g_saves_of = -1;
static hm_tile g_save_tiles[HM_SAVES_MAX];

static void duration(char *out, int cap, unsigned int s)
{
    unsigned int m = s / 60;

    if (m >= 60)
        stbsp_snprintf(out, cap, pw_tr(PW_S_DUR_HM), (int)(m / 60), (int)(m % 60));
    else if (m > 0)
        stbsp_snprintf(out, cap, pw_tr(PW_S_DUR_M), (int)m);
    else /* a short first run counts too (the owner: "nothing after a second of Castlevania") */
        stbsp_snprintf(out, cap, pw_tr(PW_S_DUR_S), (int)s);
}

static hm_row row(const char *label, const char *value, int kind)
{
    hm_row r;

    memset(&r, 0, sizeof(r));
    r.label = label;
    r.value = value;
    r.kind = kind;
    r.bar = -1;
    return r;
}

/* Add a row to b at *n with its action. */
static hm_row *add(rowbuf *b, int *n, const char *label, const char *value, int kind, int act,
                   int arg)
{
    hm_row *r;

    if (*n >= ROWS)
        return &b->r[ROWS - 1];
    r = &b->r[*n];
    *r = row(label, value, kind);
    b->act[*n].act = act;
    b->act[*n].arg = arg;
    b->act[*n].arg2 = 0;
    (*n)++;
    return r;
}

static char *val(rowbuf *b, int n)
{
    return b->val[n < ROWS ? n : ROWS - 1];
}

/* ---- Games ---- */

void hm_pages_texts(void)
{
    for (int i = 0; i < hm_library_count(); i++) {
        hm_game *g = hm_library_game(i);
        char d[32], part[48];
        int n = 0;

        g->meta[0] = 0;
        if (g->kind == HM_KIND_APP) {
            stbsp_snprintf(g->meta, sizeof(g->meta), "%s", pw_tr(PW_S_HM_HOMEBREW));
        } else if (g->kind == HM_KIND_PS1) {
            stbsp_snprintf(g->meta, sizeof(g->meta), "%s%s%s", pw_tr(PW_S_HM_PS1),
                           g->id[0] ? "   " : "", g->id);
        } else if (g->id[0])
            n += stbsp_snprintf(g->meta + n, (int)sizeof(g->meta) - n, "%s", g->id);
        if (g->play_s > 0) {
            duration(d, sizeof(d), g->play_s);
            stbsp_snprintf(part, sizeof(part), pw_tr(PW_S_HM_PLAYED), d);
            n +=
                stbsp_snprintf(g->meta + n, (int)sizeof(g->meta) - n, "%s%s", n ? "   " : "", part);
        }
        if (g->states > 0) {
            stbsp_snprintf(part, sizeof(part), pw_tr(PW_S_HM_STATES), g->states);
            n +=
                stbsp_snprintf(g->meta + n, (int)sizeof(g->meta) - n, "%s%s", n ? "   " : "", part);
        }
    }
}

static void games(hm_page *p)
{
    for (int i = 0; i < hm_library_count(); i++) {
        hm_game *g = hm_library_game(i);

        g_game_tiles[i].title = g->title;
        g_game_tiles[i].meta = g->meta[0] ? g->meta : 0;
        g_game_tiles[i].icon = g->icon;
        g_game_tiles[i].fav = g->fav;
    }
    p->layout = HM_PAGE_GAMES;
    p->tiles = g_game_tiles;
    p->count = hm_library_count();
    p->empty = pw_tr(PW_S_HM_NO_GAMES);
}

static int is_psp_game(const hm_game *g)
{
    return g && g->kind <= HM_KIND_PSN && g->id[0];
}

int hm_pages_states(int game)
{
    hm_game *g = hm_library_game(game);

    hm_states_free(g_states, g_nstates);
    g_nstates = is_psp_game(g) ? hm_states_list(g->id, g_states) : 0;
    g_states_of = game;
    for (int i = 0; i < g_nstates; i++) {
        stbsp_snprintf(g_state_names[i], sizeof(g_state_names[i]), pw_tr(PW_S_SLOT_TITLE),
                       g_states[i].slot);
        g_state_tiles[i].title = g_state_names[i];
        g_state_tiles[i].meta = g_states[i].when;
        g_state_tiles[i].icon = g_states[i].icon;
    }
    return g_nstates;
}

hm_state *hm_pages_state(int i)
{
    return i >= 0 && i < g_nstates ? &g_states[i] : 0;
}

int hm_pages_saves(int game)
{
    hm_game *g = hm_library_game(game);

    hm_library_pics(0, 0, 0); /* the loader lets go of the old pictures first */
    hm_library_pause(1);
    for (int i = 0; i < g_nsaves; i++) {
        free(g_save_pics[i].icon);
        g_save_pics[i].icon = 0;
    }
    hm_library_pause(0);
    g_nsaves = is_psp_game(g) ? hm_saves_list(g->id, g_saves, HM_SAVES_MAX) : 0;
    g_saves_of = game;
    for (int i = 0; i < g_nsaves; i++) {
        memset(&g_save_pics[i], 0, sizeof(g_save_pics[i]));
        hm_saves_path(&g_saves[i], "ICON0.PNG", g_save_pics[i].path, sizeof(g_save_pics[i].path));
        g_save_tiles[i].title = g_saves[i].title;
        g_save_tiles[i].meta = g_saves[i].detail[0] ? g_saves[i].detail : g_saves[i].meta;
    }
    hm_library_pics(g_save_pics, g_nsaves, 0);
    return g_nsaves;
}

hm_save *hm_pages_save(int i)
{
    return i >= 0 && i < g_nsaves ? &g_saves[i] : 0;
}

static void game_page(hm_page *p, int gi)
{
    rowbuf *b = &g_rows[P_GAME];
    hm_game *g = hm_library_game(gi);
    int n = 0;
    hm_row *r;

    p->layout = HM_PAGE_ROWS;
    p->over = 1; /* Classic: the XMB's Triangle panel */
    if (!g) {
        p->nrows = 0;
        return;
    }
    p->title = g->title;
    p->sub = g->meta[0] ? g->meta : 0;
    p->icon = g->icon;
    r = add(b, &n, pw_tr(PW_S_HM_PLAY), 0, HM_ROW_ACTION, A_PLAY, gi);
    r->glyph = HM_ICON_PLAY;
    if (is_psp_game(g)) {
        if (g_states_of != gi)
            hm_pages_states(gi);
        stbsp_snprintf(val(b, n), VAL, "%d", g_nstates);
        r = add(b, &n, pw_tr(PW_S_HM_STATES_PAGE), g_nstates ? val(b, n) : 0, HM_ROW_LINK, A_OPEN,
                P_STATES);
        r->glyph = PW_ICON_SAVE;
        r = add(b, &n, pw_tr(PW_S_HM_SAVED_DATA), 0, HM_ROW_LINK, A_OPEN, P_SAVES);
        r->glyph = HM_ICON_FOLDER;
    }
    r = add(b, &n, pw_tr(PW_S_HM_INFORMATION), 0, HM_ROW_LINK, A_OPEN, P_INFO);
    r->glyph = HM_ICON_SYSTEM;
    for (int i = 0; i < n; i++)
        b->act[i].arg2 = gi;
    for (int a = 1; a < hm_library_album_count(); a++) { /* its screenshots' album */
        hm_album *al = hm_library_album(a);

        if (al->game == gi && al->count > 0) {
            stbsp_snprintf(val(b, n), VAL, "%d", al->count);
            r = add(b, &n, pw_tr(PW_S_HM_PHOTOS), val(b, n), HM_ROW_LINK, A_OPEN, P_ALBUM);
            r->glyph = HM_ICON_PHOTOS;
            b->act[n - 1].arg2 = a;
            break;
        }
    }
    p->rows = b->r;
    p->nrows = n;
}

static void info_page(hm_page *p, int gi)
{
    rowbuf *b = &g_rows[P_INFO];
    hm_game *g = hm_library_game(gi);
    const char *slash;
    int n = 0;

    p->layout = HM_PAGE_ROWS;
    p->title = pw_tr(PW_S_HM_INFORMATION);
    p->rows = b->r;
    if (!g) {
        p->nrows = 0;
        return;
    }
    p->sub = g->title;
    if (g->id[0])
        add(b, &n, pw_tr(PW_S_HM_DISC_ID), g->id, HM_ROW_INFO, A_NONE, 0);
    if (g->version[0])
        add(b, &n, pw_tr(PW_S_HM_VERSION), g->version, HM_ROW_INFO, A_NONE, 0);
    if (g->sysver[0])
        add(b, &n, pw_tr(PW_S_HM_FIRMWARE), g->sysver, HM_ROW_INFO, A_NONE, 0);
    if (g->size_kb >= 1024)
        stbsp_snprintf(val(b, n), VAL, "%u MB", (g->size_kb + 512) / 1024);
    else
        stbsp_snprintf(val(b, n), VAL, "%u KB", g->size_kb);
    add(b, &n, pw_tr(PW_S_HM_SIZE), val(b, n), HM_ROW_INFO, A_NONE, 0);
    if (g->play_s > 0) {
        duration(val(b, n), VAL, g->play_s);
        add(b, &n, pw_tr(PW_S_PLAYTIME), val(b, n), HM_ROW_INFO, A_NONE, 0);
    }
    slash = strrchr(g->path, '/');
    add(b, &n, pw_tr(PW_S_HM_FILE), slash ? slash + 1 : g->path, HM_ROW_INFO, A_NONE, 0);
    p->nrows = n;
}

static void states_page(hm_page *p, int gi)
{
    if (g_states_of != gi)
        hm_pages_states(gi);
    p->layout = HM_PAGE_SAVES;
    p->title = pw_tr(PW_S_HM_STATES_PAGE);
    p->sub = hm_library_game(gi) ? hm_library_game(gi)->title : 0;
    p->tiles = g_state_tiles;
    p->count = g_nstates;
    p->empty = pw_tr(PW_S_NO_STATES);
}

static void saves_page(hm_page *p, int gi)
{
    if (g_saves_of != gi)
        hm_pages_saves(gi);
    for (int i = 0; i < g_nsaves; i++)
        g_save_tiles[i].icon = g_save_pics[i].icon;
    p->layout = HM_PAGE_SAVES;
    p->title = pw_tr(PW_S_HM_SAVED_DATA);
    p->sub = hm_library_game(gi) ? hm_library_game(gi)->title : 0;
    p->tiles = g_save_tiles;
    p->count = g_nsaves;
    p->empty = pw_tr(PW_S_HM_NO_SAVES);
}

static void save_page(hm_page *p, int si)
{
    rowbuf *b = &g_rows[P_SAVE];
    hm_save *s = hm_pages_save(si);
    int n = 0;
    hm_row *r;

    p->layout = HM_PAGE_ROWS;
    p->rows = b->r;
    if (!s) {
        p->nrows = 0;
        return;
    }
    p->title = s->title;
    p->icon = g_save_pics[si].icon;
    p->sub = s->detail[0] ? s->detail : 0;
    if (!p->icon) /* (no picture: the name line instead) */
        p->sub = s->game[0] ? s->game : 0;
    if (s->game[0])
        add(b, &n, pw_tr(PW_S_HM_SD_GAME), s->game, HM_ROW_INFO, A_NONE, 0);
    if (s->detail[0])
        add(b, &n, pw_tr(PW_S_HM_SD_DETAIL), s->detail, HM_ROW_INFO, A_NONE, 0);
    add(b, &n, pw_tr(PW_S_HM_SD_DATE), s->when, HM_ROW_INFO, A_NONE, 0);
    if (s->size_kb >= 1024)
        stbsp_snprintf(val(b, n), VAL, "%u.%u MB", s->size_kb / 1024,
                       s->size_kb % 1024 * 10 / 1024);
    else
        stbsp_snprintf(val(b, n), VAL, "%u KB", s->size_kb);
    add(b, &n, pw_tr(PW_S_HM_SIZE), val(b, n), HM_ROW_INFO, A_NONE, 0);
    add(b, &n, pw_tr(PW_S_HM_SD_FOLDER), s->dir, HM_ROW_INFO, A_NONE, 0);
    r = add(b, &n, pw_tr(PW_S_DELETE), 0, HM_ROW_ACTION, A_DELETE, si);
    r->glyph = HM_ICON_TRASH;
    p->nrows = n;
}

/* ---- Music ---- */

static void music_page(hm_view *v, hm_page *p)
{
    int cur = hm_music_current(), n = hm_music_count(), rows = 0, bar = -1;
    unsigned int played, length;
    const hm_track *t = cur >= 0 ? hm_music_track(cur) : 0;

    hm_music_time(&played, &length);
    if (t && length > 0)
        bar = (int)(played * 256 / length > 256 ? 256 : played * 256 / length);
    stbsp_snprintf(g_mu_now, sizeof(g_mu_now), "%u:%02u", played / 60, played % 60);
    stbsp_snprintf(g_mu_len, sizeof(g_mu_len), "%u:%02u", length / 60, length % 60);
    g_mu_rows[rows] =
        row(t   ? t->title
            : n ? pw_tr(PW_S_HM_MU_IDLE)
                : pw_tr(PW_S_HM_MU_NONE),
            t ? (hm_music_paused() ? pw_tr(PW_S_HM_MU_PAUSED) : g_mu_now) : 0, HM_ROW_INFO);
    g_mu_rows[rows].bar = bar;
    g_mu_act[rows].act = A_OPEN;
    g_mu_act[rows++].arg = P_NOW;
    v->mu_title = t ? t->title : 0;
    v->mu_artist = t && t->artist[0] ? t->artist : 0;
    v->mu_album = t && t->album[0] ? t->album : 0;
    v->mu_time = t ? g_mu_now : 0;
    v->mu_length = t && length ? g_mu_len : 0;
    v->mu_bar = bar;
    v->mu_state = !t ? 0 : hm_music_paused() ? 2 : 1;
    v->mu_cur = cur;
    v->mu_shuffle = hm_cfg_get()->shuffle;
    v->mu_repeat = hm_cfg_get()->repeat;
    for (int i = 0; i < n; i++) {
        const hm_track *k = hm_music_track(i);

        g_mu_rows[rows] = row(k->title, k->artist[0] ? k->artist : 0, HM_ROW_ACTION);
        g_mu_rows[rows].bar = i == cur ? bar : -1;
        g_mu_act[rows].act = A_TRACK;
        g_mu_act[rows++].arg = i;
    }
    p->layout = HM_PAGE_MUSIC;
    p->rows = g_mu_rows;
    p->nrows = rows;
}

/* ---- Photos ---- */

int hm_pages_album(int album)
{
    g_album_n = hm_library_album_photos(album, g_album_list, HM_PHOTOS_MAX);
    g_album_of = album;
    return g_album_n;
}

int hm_pages_album_photo(int k)
{
    return k >= 0 && k < g_album_n ? g_album_list[k] : -1;
}

static void albums_page(hm_page *p)
{
    int n = hm_library_album_count(), k = 0;

    for (int a = 0; a < n && k < HM_ALBUMS_MAX; a++) {
        hm_album *al = hm_library_album(a);
        hm_photo *cover = al ? hm_library_photo(al->cover) : 0;

        if (!al || (al->count == 0 && a > 0))
            continue;
        stbsp_snprintf(g_album_meta[k], sizeof(g_album_meta[k]), pw_tr(PW_S_HM_N_PHOTOS),
                       al->count);
        g_album_tiles[k].title = a == 0 ? pw_tr(PW_S_HM_ALL_PHOTOS)
                                 : strcmp(al->folder, "SCREENSHOT") == 0
                                     ? pw_tr(PW_S_HM_SCREENSHOTS)
                                     : al->title;
        g_album_tiles[k].meta = g_album_meta[k];
        g_album_tiles[k].icon = cover ? cover->thumb : 0;
        k++;
    }
    p->layout = HM_PAGE_ALBUMS;
    p->tiles = g_album_tiles;
    p->count = hm_library_photo_count() ? k : 0;
    p->empty = pw_tr(PW_S_HM_PH_NONE);
}

/* The album shown at tile k (albums with no pictures are not shown). */
int hm_pages_album_at(int k)
{
    int n = hm_library_album_count(), seen = 0;

    for (int a = 0; a < n; a++) {
        hm_album *al = hm_library_album(a);

        if (!al || (al->count == 0 && a > 0))
            continue;
        if (seen++ == k)
            return a;
    }
    return 0;
}

static void album_page(hm_page *p, int a)
{
    hm_album *al = hm_library_album(a);

    if (g_album_of != a)
        hm_pages_album(a);
    for (int i = 0; i < g_album_n; i++) {
        hm_photo *ph = hm_library_photo(g_album_list[i]);

        g_photo_tiles[i] = (hm_tile){ph->name, ph->meta, ph->thumb, 0};
    }
    p->layout = HM_PAGE_PHOTOS;
    p->title = a == 0                                        ? pw_tr(PW_S_HM_ALL_PHOTOS)
               : al && strcmp(al->folder, "SCREENSHOT") == 0 ? pw_tr(PW_S_HM_SCREENSHOTS)
               : al                                          ? al->title
                                                             : "";
    p->tiles = g_photo_tiles;
    p->count = g_album_n;
    p->empty = pw_tr(PW_S_HM_PH_NONE);
}

/* ---- Play Time ---- */

static void playtime_page(hm_page *p)
{
    int order[HM_GAMES_MAX], n = 0, rows = 0;
    unsigned int total = 0, most = 1;

    for (int i = 0; i < hm_library_count(); i++)
        if (hm_library_game(i)->play_s > 0) {
            int k = n++;
            unsigned int s = hm_library_game(i)->play_s;

            while (k > 0 && hm_library_game(order[k - 1])->play_s < s) {
                order[k] = order[k - 1];
                k--;
            }
            order[k] = i;
            total += s;
            if (s > most)
                most = s;
        }
    duration(g_pt_vals[rows], 48, total);
    g_pt_rows[rows] =
        row(pw_tr(PW_S_PT_TOTAL), n ? g_pt_vals[0] : pw_tr(PW_S_PT_NONE), HM_ROW_INFO);
    g_pt_act[rows++].act = A_NONE;
    for (int i = 0; i < n && rows < HM_GAMES_MAX + 1; i++) {
        hm_game *g = hm_library_game(order[i]);

        duration(g_pt_vals[rows], 48, g->play_s);
        g_pt_rows[rows] = row(g->title, g_pt_vals[rows], HM_ROW_INFO);
        g_pt_rows[rows].bar = (int)(g->play_s * 256ull / most);
        g_pt_rows[rows].icon = g->icon;
        g_pt_act[rows].act = A_OPEN; /* its page */
        g_pt_act[rows].arg = P_GAME;
        g_pt_act[rows++].arg2 = order[i];
    }
    g_pt_rows[rows] = row(pw_tr(PW_S_PT_NOTE), 0, HM_ROW_NOTE);
    g_pt_act[rows++].act = A_NONE;
    p->layout = HM_PAGE_ROWS;
    p->rows = g_pt_rows;
    p->nrows = rows;
}

/* ---- Settings ---- */

static void settings_page(hm_page *p)
{
    rowbuf *b = &g_rows[P_SETTINGS];
    int n = 0;

    for (int c = 0; c < HS_CATS; c++) {
        hm_row *r;

        hs_cat_value(c, val(b, n), VAL);
        r = add(b, &n, hs_cat_label(c), val(b, n - 0)[0] ? val(b, n) : 0, HM_ROW_LINK, A_OPEN,
                P_SET_CAT);
        r->glyph = hs_cat_glyph(c);
        r->art = hs_cat_art(c);
        b->act[n - 1].arg2 = c;
    }
    p->layout = HM_PAGE_ROWS;
    p->rows = b->r;
    p->nrows = n;
}

static void category_page(hm_page *p, int c)
{
    static const int kinds[3] = {HM_ROW_HEADER, HM_ROW_CHOICE, HM_ROW_ACTION};
    rowbuf *b = &g_rows[P_SET_CAT];
    int idx[ROWS], k = hs_cat_rows(c, idx, ROWS), n = 0;

    for (int i = 0; i < k; i++) {
        hm_row *r;
        int cur;

        hs_value(idx[i], val(b, n), VAL);
        r = add(b, &n, hs_label(idx[i]), val(b, n)[0] ? val(b, n) : 0, kinds[hs_kind(idx[i])],
                hs_action(idx[i]) == HS_ACT_NICKNAME ? A_NICKNAME : A_SETTING, idx[i]);
        r->toggle = hs_toggle(idx[i]);
        cur = hs_current(idx[i]);
        r->swatch = hs_swatch(idx[i], cur);
    }
    p->layout = HM_PAGE_ROWS;
    p->title = hs_cat_label(c);
    p->rows = b->r;
    p->nrows = n;
}

/* ---- System ---- */

static void system_page(hm_page *p, const hm_page_env *env)
{
    rowbuf *b = &g_rows[P_SYSTEM];
    int n = 0;
    hm_battery bat;
    hm_row *r;

    r = add(b, &n, pw_tr(PW_S_HM_USB), 0, HM_ROW_ACTION, A_USB, 0);
    r->glyph = HM_ICON_USB;
    r->art = HM_ART_USB + 1;
    r = add(b, &n, pw_tr(PW_S_HM_BROWSER), env->web_failed ? pw_tr(PW_S_BAT_NA) : 0, HM_ROW_ACTION,
            A_BROWSER, 0);
    r->glyph = HM_ICON_GLOBE;
    r->art = HM_ART_WWW + 1;
    hm_hw_battery(&bat);
    if (bat.pct >= 0)
        stbsp_snprintf(val(b, n), VAL, "%d%%", bat.pct);
    r = add(b, &n, pw_tr(PW_S_BATTERY), bat.pct >= 0 ? val(b, n) : 0, HM_ROW_LINK, A_OPEN,
            P_SYS_BATTERY);
    r->glyph = HM_ICON_BATTERY;
    if (hm_library_free_mb() >= 0)
        stbsp_snprintf(val(b, n), VAL, pw_tr(PW_S_HM_FREE_MB), hm_library_free_mb());
    r = add(b, &n, pw_tr(PW_S_HM_STORAGE), hm_library_free_mb() >= 0 ? val(b, n) : 0, HM_ROW_LINK,
            A_OPEN, P_SYS_STORAGE);
    r->glyph = PW_ICON_CARD;
    r->art = HM_ART_MS + 1;
    r = add(b, &n, pw_tr(PW_S_HM_ABOUT), env->model, HM_ROW_LINK, A_OPEN, P_SYS_ABOUT);
    r->glyph = HM_ICON_SYSTEM;
    r->art = HM_ART_SYSTEM + 1;
    r = add(b, &n, pw_tr(PW_S_HM_POWER), 0, HM_ROW_LINK, A_OPEN, P_SYS_POWER);
    r->glyph = PW_ICON_QUIT;
    r->art = HM_ART_POWER + 1;
    r = add(b, &n, pw_tr(PW_S_HM_EXIT), 0, HM_ROW_ACTION, A_EXIT, 0);
    r->glyph = HM_ICON_EXIT;
    p->layout = HM_PAGE_ROWS;
    p->rows = b->r;
    p->nrows = n;
}

static void battery_page(hm_page *p)
{
    rowbuf *b = &g_rows[P_SYS_BATTERY];
    int n = 0;
    hm_battery bat;

    hm_hw_battery(&bat); /* (read by hw.c's thread: these calls are slow) */
    if (bat.pct >= 0)
        stbsp_snprintf(val(b, n), VAL, "%d%%   %s", bat.pct,
                       pw_tr(bat.charging ? PW_S_BAT_CHARGING : PW_S_BAT_ON_BATTERY));
    add(b, &n, pw_tr(PW_S_BAT_CHARGE), bat.pct >= 0 ? val(b, n) : pw_tr(PW_S_BAT_NA), HM_ROW_INFO,
        A_NONE, 0)
        ->bar = bat.pct >= 0 ? bat.pct * 256 / 100 : -1;
    if (!bat.charging && bat.left_min > 0 && bat.left_min < 24 * 60) {
        char d[32];

        duration(d, sizeof(d), (unsigned int)bat.left_min * 60);
        stbsp_snprintf(val(b, n), VAL, pw_tr(PW_S_BAT_SYSTEM), d);
        add(b, &n, pw_tr(PW_S_BAT_LEFT), val(b, n), HM_ROW_INFO, A_NONE, 0);
    }
    if (bat.mv > 0) {
        stbsp_snprintf(val(b, n), VAL, "%d.%02d V", bat.mv / 1000, bat.mv % 1000 / 10);
        add(b, &n, pw_tr(PW_S_BAT_VOLTAGE), val(b, n), HM_ROW_INFO, A_NONE, 0);
    }
    if (bat.temp > 0 && bat.temp < 80) {
        stbsp_snprintf(val(b, n), VAL,
                       "%d \xC2\xB0"
                       "C",
                       bat.temp); /* U+00B0 */
        add(b, &n, pw_tr(PW_S_BAT_TEMPERATURE), val(b, n), HM_ROW_INFO, A_NONE, 0);
    }
    if (bat.rcap > 0 && bat.full > 0) {
        stbsp_snprintf(val(b, n), VAL, pw_tr(PW_S_BAT_CAPACITY_V), bat.rcap, bat.full);
        add(b, &n, pw_tr(PW_S_BAT_CAPACITY), val(b, n), HM_ROW_INFO, A_NONE, 0);
    }
    if (bat.cycles >= 0) {
        stbsp_snprintf(val(b, n), VAL, "%d", bat.cycles);
        add(b, &n, pw_tr(PW_S_BAT_CYCLES), val(b, n), HM_ROW_INFO, A_NONE, 0);
    }
    p->layout = HM_PAGE_ROWS;
    p->title = pw_tr(PW_S_BATTERY);
    p->rows = b->r;
    p->nrows = n;
}

static void storage_page(hm_page *p)
{
    rowbuf *b = &g_rows[P_SYS_STORAGE];
    int n = 0;

    if (hm_library_free_mb() >= 0)
        stbsp_snprintf(val(b, n), VAL, "%d MB", hm_library_free_mb());
    else
        stbsp_snprintf(val(b, n), VAL, "...");
    add(b, &n, pw_tr(PW_S_HM_FREE), val(b, n), HM_ROW_INFO, A_NONE, 0);
    stbsp_snprintf(val(b, n), VAL, "%d", hm_library_count());
    add(b, &n, pw_tr(PW_S_HM_INSTALLED), val(b, n), HM_ROW_INFO, A_NONE, 0);
    stbsp_snprintf(val(b, n), VAL, "%d", hm_library_photo_count());
    add(b, &n, pw_tr(PW_S_HM_PHOTOS), val(b, n), HM_ROW_INFO, A_NONE, 0);
    stbsp_snprintf(val(b, n), VAL, "%d", hm_music_count());
    add(b, &n, pw_tr(PW_S_HM_MUSIC), val(b, n), HM_ROW_INFO, A_NONE, 0);
    p->layout = HM_PAGE_ROWS;
    p->title = pw_tr(PW_S_HM_STORAGE);
    p->rows = b->r;
    p->nrows = n;
}

static void about_page(hm_page *p, const hm_page_env *env)
{
    rowbuf *b = &g_rows[P_SYS_ABOUT];
    int n = 0;

    hs_nickname(val(b, n), VAL);
    add(b, &n, pw_tr(PW_S_HS_NICKNAME), val(b, n), HM_ROW_INFO, A_NONE, 0);
    add(b, &n, pw_tr(PW_S_HM_MODEL), env->model, HM_ROW_INFO, A_NONE, 0);
    add(b, &n, pw_tr(PW_S_HM_FIRMWARE), env->firmware, HM_ROW_INFO, A_NONE, 0);
    add(b, &n, pw_tr(PW_S_HM_CFW), env->cfw, HM_ROW_INFO, A_NONE, 0);
    if (env->mac && env->mac[0])
        add(b, &n, pw_tr(PW_S_HM_MAC), env->mac, HM_ROW_INFO, A_NONE, 0);
    add(b, &n, pw_tr(PW_S_HM_VERSION), "Pocketwatch Home " PW_VERSION, HM_ROW_INFO, A_NONE, 0);
    p->layout = HM_PAGE_ROWS;
    p->title = pw_tr(PW_S_HM_ABOUT);
    p->rows = b->r;
    p->nrows = n;
}

static void power_page(hm_page *p)
{
    rowbuf *b = &g_rows[P_SYS_POWER];
    int n = 0;

    add(b, &n, pw_tr(PW_S_HM_SLEEP), 0, HM_ROW_ACTION, A_SLEEP, 0)->glyph = HM_ICON_SLEEP;
    add(b, &n, pw_tr(PW_S_HM_POWER_OFF), 0, HM_ROW_ACTION, A_OFF, 0)->glyph = PW_ICON_QUIT;
    add(b, &n, pw_tr(PW_S_HM_EXIT), 0, HM_ROW_ACTION, A_EXIT, 0)->glyph = HM_ICON_EXIT;
    p->layout = HM_PAGE_ROWS;
    p->title = pw_tr(PW_S_HM_POWER);
    p->rows = b->r;
    p->nrows = n;
}

/* ---- the stack ---- */

int hm_page_root(int tab)
{
    static const int k[HM_TABS] = {P_GAMES, P_MUSIC, P_ALBUMS, P_PLAYTIME, P_SETTINGS, P_SYSTEM};

    return tab >= 0 && tab < HM_TABS ? k[tab] : P_GAMES;
}

void hm_page_build(hm_view *v, int d, hm_page_id id, const hm_page_env *env)
{
    hm_page *p = &v->page[d];
    int row_sel = p->row_sel, row_pos = p->row_pos, sel = p->sel, selpos = p->selpos,
        grid_pos = p->grid_pos;

    memset(p, 0, sizeof(*p));
    p->row_sel = row_sel;
    p->row_pos = row_pos;
    p->sel = sel;
    p->selpos = selpos;
    p->grid_pos = grid_pos;
    switch (id.id) {
    case P_GAMES:
        games(p);
        break;
    case P_GAME:
        game_page(p, id.arg);
        break;
    case P_STATES:
        states_page(p, id.arg);
        break;
    case P_SAVES:
        saves_page(p, id.arg);
        break;
    case P_SAVE:
        save_page(p, id.arg);
        break;
    case P_INFO:
        info_page(p, id.arg);
        break;
    case P_MUSIC:
        music_page(v, p);
        break;
    case P_NOW:
        music_page(v, p); /* (the same words) */
        p->layout = HM_PAGE_NOW;
        p->title = pw_tr(PW_S_HM_NOW);
        p->rows = 0;
        p->nrows = 0;
        p->empty = pw_tr(PW_S_HM_MU_IDLE);
        break;
    case P_ALBUMS:
        albums_page(p);
        break;
    case P_ALBUM:
        album_page(p, id.arg);
        break;
    case P_PLAYTIME:
        playtime_page(p);
        break;
    case P_SETTINGS:
        settings_page(p);
        break;
    case P_SET_CAT:
        category_page(p, id.arg);
        break;
    case P_SYSTEM:
        system_page(p, env);
        break;
    case P_SYS_BATTERY:
        battery_page(p);
        break;
    case P_SYS_STORAGE:
        storage_page(p);
        break;
    case P_SYS_ABOUT:
        about_page(p, env);
        break;
    case P_SYS_POWER:
        power_page(p);
        break;
    default:
        break;
    }
    /* (a list that got shorter: the cursor stays on it) */
    if (p->nrows > 0 && p->row_sel >= p->nrows)
        p->row_sel = p->nrows - 1;
    if (p->count > 0 && p->sel >= p->count)
        p->sel = p->count - 1;
}

hm_row_act hm_page_act(hm_page_id id, int r)
{
    hm_row_act none = {A_NONE, 0, 0};

    if (r < 0)
        return none;
    if (id.id == P_MUSIC)
        return r <= HM_TRACKS_MAX ? g_mu_act[r] : none;
    if (id.id == P_PLAYTIME)
        return r < HM_GAMES_MAX + 2 ? g_pt_act[r] : none;
    if (id.id < 0 || id.id >= P_COUNT || r >= ROWS)
        return none;
    return g_rows[id.id].act[r];
}

void hm_pages_drop(void)
{
    hm_library_pics(0, 0, 0);
    hm_states_free(g_states, g_nstates);
    g_nstates = 0;
    g_states_of = -1;
    hm_library_pause(1);
    for (int i = 0; i < g_nsaves; i++) {
        free(g_save_pics[i].icon);
        g_save_pics[i].icon = 0;
    }
    hm_library_pause(0);
    g_nsaves = 0;
    g_saves_of = -1;
    g_album_of = -1;
}
