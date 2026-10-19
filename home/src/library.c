// SPDX-License-Identifier: GPL-3.0-or-later
#include <malloc.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <stdlib.h>
#include <string.h>

#include "complications/states/sfo.h"
#include "core/playtime.h"
#include "disc.h"
#include "gfx_gu.h"
#include "hm_image.h"
#include "hw.h"
#include "library.h"
#include "stb_sprintf.h"

#define ISO_DIR      "ms0:/ISO"
#define GAME_DIR     "ms0:/PSP/GAME"
#define HOME_FOLDER  "PocketwatchHome" /* this application: not listed */
#define SAVEDATA_DIR "ms0:/PSP/SAVEDATA"
#define PW_DIR       "ms0:/seplugins/pocketwatch/"
#define SFO_MAX      4096
#define ICON_MAX     (128 * 1024)
#define PIC_MAX      (768 * 1024)
#define SETTLE_MS    350 /* the background waits until the choice stops moving */

static hm_game g_games[HM_GAMES_MAX];
static int g_count;
static volatile int g_focus, g_focus_ms;
static unsigned int *g_bg[2]; /* two buffers: the screen may still mix from the older one */
static volatile int g_bg_game[2] = {-1, -1};
static volatile int g_pause, g_idle;    /* USB: the loader keeps off the card */
static SceUID g_thread = -1;            /* the loader, once started */
static volatile unsigned int g_free_mb; /* 0: not counted yet */
static hm_photo g_photos[HM_PHOTOS_MAX];
static int g_photo_count;
static hm_album g_albums[HM_ALBUMS_MAX];
static int g_album_count;
static volatile int g_mode, g_photo_focus; /* hm_library_mode */
static int g_view[HM_PHOTOS_MAX];          /* hm_library_photo_view */
static volatile int g_view_n, g_view_pos;
static hm_pic_job *volatile g_jobs; /* hm_library_pics */
static volatile int g_jobs_n, g_jobs_focus;
static char g_file_bg[160]; /* hm_library_file_bg */
static volatile int g_file_bg_seq, g_file_bg_done = -1;
#define TAG_FILE  30000     /* g_bg_game: the file background (+ its request) */
#define TAG_COVER 40000     /* g_bg_game: a cover's background (+ its request) */
#define COVER_MAX (2 << 20) /* bytes of a tag's picture at most */
static char g_cover_path[192];
static unsigned int g_cover_off, g_cover_len;
static volatile int g_cover_seq, g_cover_done = -1, g_cover_found;
static unsigned int *g_cover_px; /* HM_COVER x HM_COVER, made once */
#define PHOTO_FILE_MAX (6 << 20) /* bytes of one picture file at most */
#define PHOTO_PIXELS   (2600000) /* pixels of one picture at most (decoded: 4 bytes each) */
#define TAG_PHOTO      10000     /* g_bg_game: a photo's background (+ its index) */
#define TAG_WHOLE      20000     /* g_bg_game: a photo shown whole (+ its index) */
static volatile int g_free_want = 1;

/* ---- one image file ---- */

static int read_fd(void *ctx, unsigned int offset, void *buf, unsigned int len)
{
    SceUID fd = (SceUID)(long)ctx;

    if (sceIoLseek32(fd, (int)offset, PSP_SEEK_SET) < 0)
        return -1;
    return sceIoRead(fd, buf, len);
}

enum { PART_SFO, PART_ICON0, PART_PIC1 };

/* One part of a game - PARAM.SFO, ICON0, PIC1 - into a new buffer (free it); its size in *len.
   NULL when missing. From the image's PSP_GAME/, or from the EBOOT.PBP's table. */
static unsigned char *load(int kind, const char *path, int part, unsigned int cap, int *len)
{
    static const char *const inner[3] = {"PSP_GAME/PARAM.SFO", "PSP_GAME/ICON0.PNG",
                                         "PSP_GAME/PIC1.PNG"};
    static const int pbp[3] = {HM_PBP_SFO, HM_PBP_ICON0, HM_PBP_PIC1};
    static hm_disc d; /* one at a time per thread: the loader and the scan never overlap */
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    unsigned int off = 0, size = 0;
    unsigned char *buf = 0, hdr[HM_PBP_HEADER];
    int found;

    if (fd < 0)
        return 0;
    if (kind == HM_KIND_ISO) {
        found = hm_disc_open(&d, read_fd, hm_inflate_raw, (void *)(long)fd) == 0 &&
                hm_disc_find(&d, inner[part], &off, &size) == 0;
    } else {
        int file = (int)sceIoLseek32(fd, 0, PSP_SEEK_END);

        found = file > 0 && read_fd((void *)(long)fd, 0, hdr, sizeof(hdr)) == (int)sizeof(hdr) &&
                hm_pbp_part(hdr, (unsigned int)file, pbp[part], &off, &size) == 0;
    }
    if (found && size > 0 && size <= cap && (buf = malloc(size + 1)) != 0 &&
        (kind == HM_KIND_ISO ? hm_disc_read(&d, off, buf, size)
                             : read_fd((void *)(long)fd, off, buf, size)) != (int)size) {
        free(buf);
        buf = 0;
    }
    sceIoClose(fd);
    *len = buf ? (int)size : 0;
    return buf;
}

static void sfo_string(const unsigned char *sfo, int len, const char *key, char *out, int cap)
{
    unsigned int off, n;

    out[0] = 0;
    if (pw_sfo_find(sfo, (unsigned int)len, key, &off, &n) < 0)
        return;
    if (n >= (unsigned int)cap)
        n = (unsigned int)cap - 1;
    memcpy(out, sfo + off, n);
    out[n] = 0;
}

static int ends_with(const char *s, const char *ext)
{
    int n = (int)strlen(s), m = (int)strlen(ext);

    if (n < m)
        return 0;
    for (int i = 0; i < m; i++) {
        char a = s[n - m + i], b = ext[i];

        if (a >= 'A' && a <= 'Z')
            a = (char)(a - 'A' + 'a');
        if (a != b)
            return 0;
    }
    return 1;
}

/* Add the game at path; name is what it is listed as when its PARAM.SFO has no title. */
static void add(int kind, const char *path, const char *name)
{
    hm_game *g;
    unsigned char *sfo;
    char category[8] = "";
    int len;

    if (g_count >= HM_GAMES_MAX)
        return;
    g = &g_games[g_count];
    memset(g, 0, sizeof(*g));
    g->kind = kind;
    stbsp_snprintf(g->path, sizeof(g->path), "%s", path);
    {
        SceIoStat st;

        if (sceIoGetstat(path, &st) >= 0)
            g->size_kb = (unsigned int)((st.st_size + 1023) >> 10);
    }
    if ((sfo = load(kind, path, PART_SFO, SFO_MAX, &len)) != 0) {
        sfo_string(sfo, len, "TITLE", g->title, sizeof(g->title));
        sfo_string(sfo, len, "DISC_ID", g->id, sizeof(g->id));
        sfo_string(sfo, len, "CATEGORY", category, sizeof(category));
        sfo_string(sfo, len, "DISC_VERSION", g->version, sizeof(g->version));
        sfo_string(sfo, len, "PSP_SYSTEM_VER", g->sysver, sizeof(g->sysver));
        free(sfo);
    }
    if (kind != HM_KIND_ISO) {
        if (strcmp(category, "ME") == 0)
            g->kind = HM_KIND_PS1;
        else if (strcmp(category, "EG") == 0)
            g->kind = HM_KIND_PSN; /* a PSN game's EBOOT */
        else if (category[0] && strcmp(category, "MG") != 0)
            return; /* an update, a UMD game's EBOOT: not started from here */
        else
            g->kind = HM_KIND_APP;
    }
    if (!g->title[0]) /* no PARAM.SFO title: listed by its file or folder name */
        stbsp_snprintf(g->title, sizeof(g->title), "%s", name);
    g_count++;
}

static void add_image(const char *path)
{
    const char *slash = strrchr(path, '/');

    add(HM_KIND_ISO, path, slash ? slash + 1 : path);
}

/* ms0:/PSP/GAME/<folder>/EBOOT.PBP */
static void scan_apps(void)
{
    SceUID dd = sceIoDopen(GAME_DIR);
    SceIoDirent e;

    if (dd < 0)
        return;
    memset(&e, 0, sizeof(e));
    while (sceIoDread(dd, &e) > 0) {
        char path[192];
        SceIoStat st;

        if (FIO_S_ISDIR(e.d_stat.st_mode) && e.d_name[0] != '.' && e.d_name[0] != '%' &&
            strncmp(e.d_name, "__SCE__", 7) != 0 && strcmp(e.d_name, HOME_FOLDER) != 0) {
            stbsp_snprintf(path, sizeof(path), "%s/%s/EBOOT.PBP", GAME_DIR, e.d_name);
            if (sceIoGetstat(path, &st) >= 0)
                add(HM_KIND_APP, path, e.d_name);
        }
        memset(&e, 0, sizeof(e));
    }
    sceIoDclose(dd);
}

static void scan_dir(const char *dir, int depth)
{
    SceUID dd = sceIoDopen(dir);
    SceIoDirent e;

    if (dd < 0)
        return;
    memset(&e, 0, sizeof(e));
    while (sceIoDread(dd, &e) > 0) {
        char path[192];

        if (e.d_name[0] == '.')
            continue;
        stbsp_snprintf(path, sizeof(path), "%s/%s", dir, e.d_name);
        if (FIO_S_ISDIR(e.d_stat.st_mode)) {
            if (depth == 0)
                scan_dir(path, 1); /* ISO/<category>/ */
        } else if (ends_with(e.d_name, ".iso") || ends_with(e.d_name, ".cso")) {
            add_image(path);
        }
        memset(&e, 0, sizeof(e));
    }
    sceIoDclose(dd);
}

/* ---- what Pocketwatch knows ---- */

static void play_times(void)
{
    pw_pt_table *t = malloc(sizeof(pw_pt_table));
    char *buf = malloc(PW_PT_TEXT_MAX);
    unsigned int best = 0;
    int which = -1;

    if (!t || !buf)
        goto out;
    for (int i = 0; i < 2; i++) {
        int n = hm_hw_read_file(i ? PW_DIR "playtime-b.txt" : PW_DIR "playtime-a.txt", buf,
                                PW_PT_TEXT_MAX);

        if (n > 0 && pw_pt_parse(t, buf, (unsigned int)n) >= 0 &&
            (which < 0 || t->seq + 1 > best)) {
            best = t->seq + 1;
            which = i;
        }
    }
    if (which >= 0) {
        int n = hm_hw_read_file(which ? PW_DIR "playtime-b.txt" : PW_DIR "playtime-a.txt", buf,
                                PW_PT_TEXT_MAX);

        if (n > 0 && pw_pt_parse(t, buf, (unsigned int)n) > 0)
            for (int i = 0; i < g_count; i++) {
                const pw_pt_game *p =
                    g_games[i].kind <= HM_KIND_PSN ? pw_pt_find(t, g_games[i].id, 0) : 0;

                if (p) {
                    g_games[i].play_s = p->total_s;
                    g_games[i].last =
                        ((((unsigned long long)p->last.year * 13 + p->last.month) * 32 +
                          p->last.day) *
                             24 +
                         p->last.hour) *
                            3600 +
                        (unsigned long long)p->last.minute * 60 + p->last.second;
                }
            }
    }
out:
    free(buf);
    free(t);
}

static void save_states(void)
{
    SceUID dd = sceIoDopen(SAVEDATA_DIR);
    SceIoDirent e;

    if (dd < 0)
        return;
    memset(&e, 0, sizeof(e));
    while (sceIoDread(dd, &e) > 0) { /* PW<disc ID><slot 01-05>: our states */
        if (e.d_name[0] == 'P' && e.d_name[1] == 'W')
            for (int i = 0; i < g_count; i++) {
                int n = (int)strlen(g_games[i].id);

                if (n && g_games[i].kind <= HM_KIND_PSN &&
                    strncmp(e.d_name + 2, g_games[i].id, (size_t)n) == 0 &&
                    strlen(e.d_name) == (size_t)n + 4)
                    g_games[i].states++;
            }
        memset(&e, 0, sizeof(e));
    }
    sceIoDclose(dd);
}

static int g_recent; /* hm_library_sort */

/* ---- favourites: seplugins/pocketwatch/favorites.txt, a game's file (its path) a line ---- */

#define FAVORITES PW_DIR "favorites.txt"
#define FAV_MAX   (16 * 1024)

static void read_favorites(void)
{
    char *buf = malloc(FAV_MAX + 1);
    int n = buf ? hm_hw_read_file(FAVORITES, buf, FAV_MAX) : -1;

    if (n > 0) {
        buf[n] = 0;
        for (int i = 0; i < g_count; i++) {
            const char *p = strstr(buf, g_games[i].path);
            size_t len = strlen(g_games[i].path);

            /* the whole line, not a part of a longer path */
            g_games[i].fav = p && (p == buf || p[-1] == '\n') &&
                             (p[len] == '\n' || p[len] == '\r' || p[len] == 0);
        }
    }
    free(buf);
}

static void write_favorites(void)
{
    SceUID fd = sceIoOpen(FAVORITES, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

    if (fd < 0)
        return;
    for (int i = 0; i < g_count; i++)
        if (g_games[i].fav) {
            sceIoWrite(fd, g_games[i].path, strlen(g_games[i].path));
            sceIoWrite(fd, "\n", 1);
        }
    sceIoClose(fd);
}

/* PSP games, PS1 games, homebrew; each by title - or, by recent, the last played first. */
static int by_kind_and_title(const void *a, const void *b)
{
    const hm_game *x = a, *y = b;
    int kx = x->kind == HM_KIND_PSN ? HM_KIND_ISO : x->kind,
        ky = y->kind == HM_KIND_PSN ? HM_KIND_ISO : y->kind;

    if (x->fav != y->fav) /* the favourites first, in the same order */
        return y->fav - x->fav;
    if (g_recent && x->last != y->last)
        return x->last > y->last ? -1 : 1;
    return kx != ky ? kx - ky : strcmp(x->title, y->title);
}

static void scan_photos(void);
static void make_albums(void);

int hm_library_scan(void)
{
    scan_photos();
    g_count = 0;
    scan_dir(ISO_DIR, 0);
    scan_apps();
    play_times();
    save_states();
    read_favorites();
    qsort(g_games, (size_t)g_count, sizeof(g_games[0]), by_kind_and_title);
    make_albums(); /* (after the games: a folder named by a disc ID gets the game's title) */
    return g_count;
}

int hm_library_has(const char *image, const char *inner)
{
    static hm_disc d;
    SceUID fd = sceIoOpen(image, PSP_O_RDONLY, 0);
    unsigned int off, size;
    int found;

    if (fd < 0)
        return 0;
    found = hm_disc_open(&d, read_fd, hm_inflate_raw, (void *)(long)fd) == 0 &&
            hm_disc_find(&d, inner, &off, &size) == 0;
    sceIoClose(fd);
    return found;
}

int hm_library_count(void)
{
    return g_count;
}

hm_game *hm_library_game(int i)
{
    return i >= 0 && i < g_count ? &g_games[i] : 0;
}

/* ---- the loader ---- */

/* An icon-sized texture of a picture: as it is when it is icon-sized, else kept in shape. */
static unsigned int *make_icon(const unsigned int *px, int w, int h)
{
    unsigned int *icon;

    if ((icon = memalign(16, HM_ICON_W * HM_ICON_H * 4)) != 0) { /* a GE texture */
        if (w == HM_ICON_W && h == HM_ICON_H) {
            memcpy(icon, px, HM_ICON_W * HM_ICON_H * 4);
        } else { /* kept in shape (a PS1 game's is 80 x 80), centred, the rest transparent */
            int fw = w * HM_ICON_H / h, fh = HM_ICON_H;
            unsigned int *fit;

            if (fw > HM_ICON_W) {
                fw = HM_ICON_W;
                fh = h * HM_ICON_W / w;
            }
            memset(icon, 0, HM_ICON_W * HM_ICON_H * 4);
            if (fw > 0 && fh > 0 && (fit = malloc((size_t)(fw * fh * 4))) != 0) {
                hm_image_fit(px, w, h, fit, fw, fh);
                for (int y = 0; y < fh; y++)
                    memcpy(icon + (y + (HM_ICON_H - fh) / 2) * HM_ICON_W + (HM_ICON_W - fw) / 2,
                           fit + y * fw, (size_t)fw * 4);
                free(fit);
            }
        }
        hm_gu_written(icon, HM_ICON_W * HM_ICON_H * 4);
    }
    return icon;
}

static void load_icon(hm_game *g)
{
    int len, w = 0, h = 0;
    unsigned char *png = load(g->kind, g->path, PART_ICON0, ICON_MAX, &len);
    unsigned int *px = png ? hm_png_decode(png, len, &w, &h) : 0;

    free(png);
    g->icon_tried = 1;
    if (!px)
        return;
    g->icon = make_icon(px, w, h);
    hm_image_free(px);
}

/* A picture file decoded, if it is not too large for memory. */
static unsigned int *load_picture(const char *path, int *w, int *h)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    int size = fd >= 0 ? (int)sceIoLseek32(fd, 0, PSP_SEEK_END) : -1;
    unsigned char *file = size > 0 && size <= PHOTO_FILE_MAX ? malloc((size_t)size) : 0;
    unsigned int *px = 0;

    if (file) {
        sceIoLseek32(fd, 0, PSP_SEEK_SET);
        if (sceIoRead(fd, file, (unsigned int)size) == size &&
            hm_image_info(file, size, w, h) == 0 && (long long)*w * *h <= PHOTO_PIXELS)
            px = hm_png_decode(file, size, w, h);
        free(file);
    }
    if (fd >= 0)
        sceIoClose(fd);
    return px;
}

static void load_thumb(hm_photo *p)
{
    int w = 0, h = 0;
    unsigned int *px = load_picture(p->path, &w, &h);

    p->tried = 1;
    if (px) {
        p->thumb = make_icon(px, w, h);
        hm_image_free(px);
    }
}

static int g_next;           /* the buffer the next background goes to */
static volatile int g_sharp; /* Classic: a game's PIC1 as the XMB shows it, not blurred */

/* A photo's background: blurred behind the row, or whole for the viewer. */
static void load_photo_background(int sel, int whole)
{
    int slot = g_next, w = 0, h = 0;
    unsigned int *px = load_picture(g_photos[sel].path, &w, &h);

    if (px) { /* (the editor needs it) */
        g_photos[sel].w = w;
        g_photos[sel].h = h;
    }
    if (px && g_bg[slot]) {
        g_bg_game[slot] = -2; /* being written */
        if (whole)
            hm_make_photo(px, w, h, g_bg[slot]);
        else
            hm_make_background(px, w, h, g_bg[slot]);
        hm_gu_written(g_bg[slot], 512 * HM_H * 4);
        hm_gu_upload_bg(slot, g_bg[slot]);
        g_bg_game[slot] = (whole ? TAG_WHOLE : TAG_PHOTO) + sel;
        g_next = !slot;
    }
    if (px)
        hm_image_free(px);
}

/* Into the buffer the screen does not show as the newest; the screen's cross-fade (300 ms) is
   shorter than SETTLE_MS, so it never mixes from a buffer being rewritten. */
static void load_background(int sel)
{
    int slot = g_next, len, w = 0, h = 0;
    unsigned char *png = load(g_games[sel].kind, g_games[sel].path, PART_PIC1, PIC_MAX, &len);
    unsigned int *px = png ? hm_png_decode(png, len, &w, &h) : 0;

    free(png);
    if (px && g_bg[slot]) {
        g_bg_game[slot] = -2; /* being written */
        if (g_sharp)
            hm_make_pic1(px, w, h, g_bg[slot]);
        else
            hm_make_background(px, w, h, g_bg[slot]);
        hm_gu_written(g_bg[slot], 512 * HM_H * 4);
        hm_gu_upload_bg(slot, g_bg[slot]); /* the GE reads it from VRAM, 16-bit */
        g_bg_game[slot] = sel;
        g_next = !slot;
    }
    if (px)
        hm_image_free(px);
}

/* Is picture i on screen, or near it (within d of the view's position)? */
static int in_view(int i, int pos, int d)
{
    for (int k = pos - d; k <= pos + d; k++)
        if (k >= 0 && k < g_view_n && g_view[k] == i)
            return 1;
    return 0;
}

/* One picture of the job list, the nearest to its focus: 1 when one was made. */
static int pic_job(void)
{
    hm_pic_job *jobs = g_jobs;
    int n = g_jobs_n, f = g_jobs_focus;

    for (int d = 0; jobs && d < n; d++)
        for (int s = -1; s <= 1; s += 2) {
            int k = f + s * d, w = 0, h = 0;
            unsigned int *px;

            if (k < 0 || k >= n || jobs[k].tried)
                continue;
            px = load_picture(jobs[k].path, &w, &h);
            if (g_jobs == jobs && px) /* (the list may have gone meanwhile) */
                jobs[k].icon = make_icon(px, w, h);
            if (px)
                hm_image_free(px);
            jobs[k].tried = 1;
            return 1;
        }
    return 0;
}

/* The picture's bytes: the tag's, else a cover file beside the track. */
static unsigned int *cover_picture(int *w, int *h)
{
    static const char *const names[] = {"cover.jpg", "folder.jpg",        "Folder.jpg",
                                        "front.jpg", "AlbumArtSmall.jpg", "AlbumArt.jpg",
                                        "cover.png"};
    unsigned int *px = 0;

    if (g_cover_len > 0 && g_cover_len <= COVER_MAX) {
        SceUID fd = sceIoOpen(g_cover_path, PSP_O_RDONLY, 0);
        unsigned char *buf = fd >= 0 ? malloc(g_cover_len) : 0;

        if (buf && read_fd((void *)(long)fd, g_cover_off, buf, g_cover_len) == (int)g_cover_len &&
            hm_image_info(buf, (int)g_cover_len, w, h) == 0 && (long long)*w * *h <= PHOTO_PIXELS)
            px = hm_png_decode(buf, (int)g_cover_len, w, h);
        free(buf);
        if (fd >= 0)
            sceIoClose(fd);
    }
    for (unsigned int i = 0; !px && i < sizeof(names) / sizeof(names[0]); i++) {
        char path[200];
        const char *slash = strrchr(g_cover_path, '/');

        if (!slash)
            break;
        stbsp_snprintf(path, sizeof(path), "%.*s/%s", (int)(slash - g_cover_path), g_cover_path,
                       names[i]);
        px = load_picture(path, w, h);
    }
    return px;
}

/* A track's cover: the middle square of its picture, and the picture blurred behind it all. */
static void load_cover(int seq)
{
    int slot = g_next, w = 0, h = 0;
    unsigned int *px = cover_picture(&w, &h);

    g_cover_found = 0;
    if (px && g_bg[slot] && (g_cover_px || (g_cover_px = memalign(16, HM_COVER * HM_COVER * 4)))) {
        int s = w < h ? w : h;
        unsigned int *sq = malloc((size_t)s * (size_t)s * 4);

        if (sq) {
            for (int y = 0; y < s; y++)
                memcpy(sq + y * s, px + (y + (h - s) / 2) * w + (w - s) / 2, (size_t)s * 4);
            hm_image_fit(sq, s, s, g_cover_px, HM_COVER, HM_COVER);
            free(sq);
            hm_gu_written(g_cover_px, HM_COVER * HM_COVER * 4);
            g_cover_found = 1;
        }
        g_bg_game[slot] = -2;
        hm_make_background(px, w, h, g_bg[slot]);
        hm_gu_written(g_bg[slot], 512 * HM_H * 4);
        hm_gu_upload_bg(slot, g_bg[slot]);
        g_bg_game[slot] = TAG_COVER + seq % 1000;
        g_next = !slot;
    }
    if (px)
        hm_image_free(px);
}

static void load_file_background(int seq)
{
    int slot = g_next, w = 0, h = 0;
    unsigned int *px = load_picture(g_file_bg, &w, &h);

    if (px && g_bg[slot]) {
        g_bg_game[slot] = -2;
        if (g_sharp)
            hm_make_pic1(px, w, h, g_bg[slot]);
        else
            hm_make_background(px, w, h, g_bg[slot]);
        hm_gu_written(g_bg[slot], 512 * HM_H * 4);
        hm_gu_upload_bg(slot, g_bg[slot]);
        g_bg_game[slot] = TAG_FILE + seq % 1000;
        g_next = !slot;
    }
    if (px)
        hm_image_free(px);
}

static int loader(SceSize args, void *argp)
{
    int tried_bg = -1;

    (void)args;
    (void)argp;
    for (;;) {
        int sel = g_focus, best = -1, dist = 1 << 30;

        if (g_pause) {
            g_idle = 1;
            tried_bg = -1;
            sceKernelDelayThread(20 * 1000);
            continue;
        }
        g_idle = 0;

        if (g_cover_seq != g_cover_done) { /* the cover of what plays */
            int seq = g_cover_seq;

            load_cover(seq);
            g_cover_done = seq;
            continue;
        }
        if (g_file_bg_seq != g_file_bg_done) { /* a save's PIC1 behind its list */
            int seq = g_file_bg_seq;

            load_file_background(seq);
            g_file_bg_done = seq;
            continue;
        }
        if (g_mode != 0) { /* the photos: the chosen one's background, then the thumbnails */
            int ps = g_photo_focus, tag = (g_mode == 2 ? TAG_WHOLE : TAG_PHOTO) + ps, n = g_view_n,
                vp = g_view_pos, near = -1;

            if (tag != tried_bg && ps >= 0 && ps < g_photo_count &&
                (g_mode == 2 || (int)(hm_hw_now_ms() - g_focus_ms) >= SETTLE_MS)) {
                tried_bg = tag;
                load_photo_background(ps, g_mode == 2);
                continue;
            }
            for (int d = 0; d < 12 && near < 0; d++) /* the nearest on screen without its own */
                for (int s = -1; s <= 1 && near < 0; s += 2) {
                    int k = vp + s * d;

                    if (k >= 0 && k < n && g_view[k] >= 0 && g_view[k] < g_photo_count &&
                        !g_photos[g_view[k]].tried)
                        near = g_view[k];
                }
            if (near >= 0) {
                load_thumb(&g_photos[near]);
                continue;
            }
            for (int i = 0; i < g_photo_count; i++) /* far off screen: their memory back */
                if (g_photos[i].thumb && !in_view(i, vp, 28)) {
                    unsigned int *t = g_photos[i].thumb;

                    g_photos[i].thumb = 0;
                    g_photos[i].tried = 0;
                    free(t);
                }
        } else if (sel != tried_bg && sel >= 0 && sel < g_count &&
                   (int)(hm_hw_now_ms() - g_focus_ms) >= SETTLE_MS) {
            tried_bg = sel;
            load_background(sel);
            continue;
        }
        dist = 1 << 30;
        best = -1;
        for (int i = 0; i < g_count; i++) /* the nearest icon not tried yet */
            if (!g_games[i].icon_tried) {
                int d = i > sel ? i - sel : sel - i;

                if (d < dist) {
                    dist = d;
                    best = i;
                }
            }
        if (g_mode != 0)
            best = -1; /* (the games' icons wait while the photos are shown) */
        if (best >= 0 && dist < 12) {
            load_icon(&g_games[best]);
        } else if (pic_job()) {
            continue;
        } else if (g_free_want) { /* the card's free space: seconds on a large card (its FAT is
                                     read), so here and not in the frame loop */
            g_free_want = 0;
            g_free_mb = hm_hw_free_mb() | 0x80000000u;
        } else {
            sceKernelDelayThread(30 * 1000);
        }
    }
    return 0;
}

void hm_library_start(void)
{
    SceUID th;

    for (int i = 0; i < 2; i++)
        g_bg[i] = memalign(64, 512 * HM_H * 4);
    g_focus_ms = (int)hm_hw_now_ms();
    th = sceKernelCreateThread("PocketwatchHomeLoader", loader, 0x30, 0x8000, 0, 0);
    if (th >= 0)
        sceKernelStartThread(th, 0, 0);
    g_thread = th;
}

void hm_library_focus(int sel)
{
    if (sel != g_focus) {
        g_focus = sel;
        g_focus_ms = (int)hm_hw_now_ms();
    }
}

const unsigned int *hm_library_background(int sel)
{
    for (int i = 0; i < 2; i++)
        if (g_bg_game[i] == sel)
            return g_bg[i];
    return 0;
}

void hm_library_pause(int on)
{
    g_pause = on;
    if (!on || g_thread < 0)
        return;
    g_idle = 0;
    for (int i = 0; i < 200 && !g_idle; i++) /* at most 2 s: one image being read */
        sceKernelDelayThread(10 * 1000);
}

int hm_library_rescan(void)
{
    for (int i = 0; i < g_count; i++)
        free(g_games[i].icon);
    g_bg_game[0] = g_bg_game[1] = -1;
    return hm_library_scan();
}

int hm_library_free_mb(void)
{
    return g_free_mb ? (int)(g_free_mb & 0x7FFFFFFFu) : -1;
}

void hm_library_count_free(void)
{
    g_free_want = 1;
}

/* ---- photos ---- */

static int is_picture(const char *name)
{
    static const char *const ext[] = {".png", ".jpg", ".jpeg", ".bmp"};

    for (unsigned int i = 0; i < sizeof(ext) / sizeof(ext[0]); i++)
        if (ends_with(name, ext[i]))
            return 1;
    return 0;
}

static unsigned long long when_of(const ScePspDateTime *t)
{
    return ((((unsigned long long)t->year * 13 + t->month) * 32 + t->day) * 24 + t->hour) * 3600 +
           (unsigned long long)t->minute * 60 + t->second;
}

/* The album of a folder (made when new); root: the top folder's own name. */
static int album_of(const char *folder)
{
    for (int i = 1; i < g_album_count; i++)
        if (strcmp(g_albums[i].folder, folder) == 0)
            return i;
    if (g_album_count >= HM_ALBUMS_MAX)
        return 0;
    memset(&g_albums[g_album_count], 0, sizeof(hm_album));
    stbsp_snprintf(g_albums[g_album_count].folder, sizeof(g_albums[0].folder), "%s", folder);
    return g_album_count++;
}

static const char *root = "PICTURE"; /* the album of a top folder's own pictures */

static void scan_pictures(const char *dir, const char *folder, int depth)
{
    SceUID dd = sceIoDopen(dir);
    SceIoDirent e;

    if (dd < 0)
        return;
    memset(&e, 0, sizeof(e));
    while (sceIoDread(dd, &e) > 0 && g_photo_count < HM_PHOTOS_MAX) {
        char path[192];

        if (e.d_name[0] != '.') {
            stbsp_snprintf(path, sizeof(path), "%s/%s", dir, e.d_name);
            if (FIO_S_ISDIR(e.d_stat.st_mode)) {
                if (depth < 1)
                    scan_pictures(path, e.d_name, depth + 1);
            } else if (is_picture(e.d_name)) {
                hm_photo *p = &g_photos[g_photo_count++];
                const ScePspDateTime *t = &e.d_stat.sce_st_mtime;

                memset(p, 0, sizeof(*p));
                stbsp_snprintf(p->path, sizeof(p->path), "%s", path);
                stbsp_snprintf(p->name, sizeof(p->name), "%s", e.d_name);
                stbsp_snprintf(p->meta, sizeof(p->meta), "%s%s%d/%02d/%02d", folder ? folder : "",
                               folder ? "   " : "", t->year, t->month, t->day);
                p->when = when_of(t);
                p->album = album_of(folder ? folder : root);
            }
        }
        memset(&e, 0, sizeof(e));
    }
    sceIoDclose(dd);
}

static void scan_photos(void)
{
    for (int i = 0; i < g_photo_count; i++)
        free(g_photos[i].thumb);
    g_photo_count = 0;
    g_album_count = 1; /* 0: All Photos */
    memset(&g_albums[0], 0, sizeof(hm_album));
    root = "PICTURE";
    scan_pictures("ms0:/PICTURE", 0, 0);
    root = "DCIM";
    scan_pictures("ms0:/DCIM", 0, 0);
    root = "SCREENSHOT";
    scan_pictures("ms0:/PSP/SCREENSHOT", 0, 0); /* prxshot and the like: a folder per game */
    for (int i = 1; i < g_photo_count; i++) {   /* newest first */
        hm_photo p = g_photos[i];
        int k = i;

        while (k > 0 && g_photos[k - 1].when < p.when) {
            g_photos[k] = g_photos[k - 1];
            k--;
        }
        g_photos[k] = p;
    }
}

/* The albums' counts, covers and titles (a disc ID's folder: the game's title). */
static void make_albums(void)
{
    for (int a = 0; a < g_album_count; a++) {
        hm_album *al = &g_albums[a];

        al->count = 0;
        al->cover = -1;
        al->game = -1;
        stbsp_snprintf(al->title, sizeof(al->title), "%s", al->folder);
        for (int i = 0; i < g_count && a > 0; i++)
            if (g_games[i].id[0] && strcmp(g_games[i].id, al->folder) == 0) {
                stbsp_snprintf(al->title, sizeof(al->title), "%s", g_games[i].title);
                al->game = i;
            }
    }
    for (int i = 0; i < g_photo_count; i++) {
        hm_album *al = &g_albums[g_photos[i].album];

        if (al->cover < 0)
            al->cover = i; /* (newest first) */
        al->count++;
    }
    g_albums[0].count = g_photo_count;
    g_albums[0].cover = g_photo_count ? 0 : -1;
}

int hm_library_album_count(void)
{
    return g_album_count;
}

hm_album *hm_library_album(int i)
{
    return i >= 0 && i < g_album_count ? &g_albums[i] : 0;
}

int hm_library_album_photos(int a, int *out, int cap)
{
    int n = 0;

    for (int i = 0; i < g_photo_count && n < cap; i++)
        if (a == 0 || g_photos[i].album == a)
            out[n++] = i;
    return n;
}

void hm_library_photo_removed(int i)
{
    if (i < 0 || i >= g_photo_count)
        return;
    hm_library_pause(1);
    free(g_photos[i].thumb);
    memmove(&g_photos[i], &g_photos[i + 1], sizeof(hm_photo) * (size_t)(g_photo_count - i - 1));
    g_photo_count--;
    g_bg_game[0] = g_bg_game[1] = -1;
    make_albums();
    hm_library_pause(0);
}

int hm_library_photo_added(const char *path)
{
    SceIoStat st;
    const char *slash = strrchr(path, '/');
    char folder[48] = "";
    int k;

    if (g_photo_count >= HM_PHOTOS_MAX || sceIoGetstat(path, &st) < 0 || !slash)
        return -1;
    { /* its folder's name: the album */
        const char *s = slash - 1;

        while (s > path && *s != '/')
            s--;
        stbsp_snprintf(folder, sizeof(folder), "%.*s", (int)(slash - s - 1), s + 1);
    }
    hm_library_pause(1);
    memmove(&g_photos[1], &g_photos[0], sizeof(hm_photo) * (size_t)g_photo_count); /* newest */
    memset(&g_photos[0], 0, sizeof(hm_photo));
    stbsp_snprintf(g_photos[0].path, sizeof(g_photos[0].path), "%s", path);
    stbsp_snprintf(g_photos[0].name, sizeof(g_photos[0].name), "%s", slash + 1);
    stbsp_snprintf(g_photos[0].meta, sizeof(g_photos[0].meta), "%s   %d/%02d/%02d", folder,
                   st.sce_st_mtime.year, st.sce_st_mtime.month, st.sce_st_mtime.day);
    g_photos[0].when = when_of(&st.sce_st_mtime);
    g_photos[0].album = album_of(folder);
    g_photo_count++;
    g_bg_game[0] = g_bg_game[1] = -1;
    make_albums();
    k = 0;
    hm_library_pause(0);
    return k;
}

void hm_library_photo_view(const int *list, int n, int pos)
{
    if (n > HM_PHOTOS_MAX)
        n = HM_PHOTOS_MAX;
    if (n != g_view_n || memcmp(list, g_view, sizeof(int) * (size_t)n) != 0) {
        g_view_n = 0; /* (the loader reads it as it changes: none while it does) */
        memcpy(g_view, list, sizeof(int) * (size_t)n);
        g_view_n = n;
    }
    g_view_pos = pos;
}

void hm_library_pics(hm_pic_job *jobs, int n, int focus)
{
    if (jobs != g_jobs) {
        g_jobs_n = 0;
        g_jobs = jobs;
    }
    g_jobs_n = jobs ? n : 0;
    g_jobs_focus = focus;
}

void hm_library_pics_focus(int focus)
{
    g_jobs_focus = focus;
}

void hm_library_file_bg(const char *path)
{
    if (strcmp(path, g_file_bg) == 0)
        return;
    hm_library_pause(1);
    stbsp_snprintf(g_file_bg, sizeof(g_file_bg), "%s", path);
    g_file_bg_seq++;
    hm_library_pause(0);
}

void hm_library_cover(const char *mp3, unsigned int pic_off, unsigned int pic_len)
{
    if (strcmp(mp3, g_cover_path) == 0 && pic_off == g_cover_off && pic_len == g_cover_len)
        return;
    hm_library_pause(1);
    stbsp_snprintf(g_cover_path, sizeof(g_cover_path), "%s", mp3);
    g_cover_off = pic_off;
    g_cover_len = pic_len;
    g_cover_found = 0;
    g_cover_seq++;
    hm_library_pause(0);
}

const unsigned int *hm_library_cover_pixels(void)
{
    return g_cover_done == g_cover_seq && g_cover_found ? g_cover_px : 0;
}

const unsigned int *hm_library_cover_background(void)
{
    for (int i = 0; i < 2; i++)
        if (g_bg_game[i] == TAG_COVER + g_cover_seq % 1000 && g_cover_done == g_cover_seq)
            return g_bg[i];
    return 0;
}

const unsigned int *hm_library_file_background(void)
{
    for (int i = 0; i < 2; i++)
        if (g_bg_game[i] == TAG_FILE + g_file_bg_seq % 1000 && g_file_bg_done == g_file_bg_seq)
            return g_bg[i];
    return 0;
}

int hm_library_photo_count(void)
{
    return g_photo_count;
}

hm_photo *hm_library_photo(int i)
{
    return i >= 0 && i < g_photo_count ? &g_photos[i] : 0;
}

void hm_library_mode(int mode, int sel)
{
    if (mode != g_mode || sel != g_photo_focus)
        g_focus_ms = (int)hm_hw_now_ms();
    g_photo_focus = sel;
    g_mode = mode;
}

const unsigned int *hm_library_photo_background(int sel, int whole)
{
    int tag = (whole ? TAG_WHOLE : TAG_PHOTO) + sel;

    for (int i = 0; i < 2; i++)
        if (g_bg_game[i] == tag)
            return g_bg[i];
    return 0;
}

int hm_library_toggle_fav(int i)
{
    char path[192];

    if (i < 0 || i >= g_count)
        return i;
    g_games[i].fav = !g_games[i].fav;
    stbsp_snprintf(path, sizeof(path), "%s", g_games[i].path);
    write_favorites();
    hm_library_sort(g_recent); /* (the loader paused meanwhile) */
    for (int k = 0; k < g_count; k++)
        if (strcmp(g_games[k].path, path) == 0)
            return k;
    return 0;
}

void hm_library_sort(int recent)
{
    g_recent = recent;
    hm_library_pause(1); /* the loader holds no game while they move */
    qsort(g_games, (size_t)g_count, sizeof(g_games[0]), by_kind_and_title);
    g_bg_game[0] = g_bg_game[1] = -1;
    hm_library_pause(0);
}

void hm_library_style(int sharp)
{
    if (sharp == g_sharp)
        return;
    hm_library_pause(1); /* (it tries the chosen game's background again after a pause) */
    g_sharp = sharp;
    g_bg_game[0] = g_bg_game[1] = -1;
    hm_library_pause(0);
}

int hm_library_wallpaper(const char *path, unsigned int *out)
{
    int w = 0, h = 0;
    unsigned int *px;

    hm_library_pause(1); /* one big picture at a time: the loader may hold another */
    px = load_picture(path, &w, &h);
    if (px) {
        hm_make_background(px, w, h, out);
        hm_image_free(px);
    }
    hm_library_pause(0);
    return px ? 0 : -1;
}
