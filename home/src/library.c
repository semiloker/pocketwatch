// SPDX-License-Identifier: GPL-3.0-or-later
#include <malloc.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <stdlib.h>
#include <string.h>

#include "complications/states/sfo.h"
#include "core/playtime.h"
#include "disc.h"
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
    if ((sfo = load(kind, path, PART_SFO, SFO_MAX, &len)) != 0) {
        sfo_string(sfo, len, "TITLE", g->title, sizeof(g->title));
        sfo_string(sfo, len, "DISC_ID", g->id, sizeof(g->id));
        sfo_string(sfo, len, "CATEGORY", category, sizeof(category));
        free(sfo);
    }
    if (kind != HM_KIND_ISO) {
        if (strcmp(category, "ME") == 0)
            g->kind = HM_KIND_PS1;
        else if (category[0] && strcmp(category, "MG") != 0)
            return; /* a PSN or UMD game's EBOOT, an update: not started from here (yet) */
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
                    g_games[i].kind == HM_KIND_ISO ? pw_pt_find(t, g_games[i].id, 0) : 0;

                if (p)
                    g_games[i].play_s = p->total_s;
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

                if (n && g_games[i].kind == HM_KIND_ISO &&
                    strncmp(e.d_name + 2, g_games[i].id, (size_t)n) == 0 &&
                    strlen(e.d_name) == (size_t)n + 4)
                    g_games[i].states++;
            }
        memset(&e, 0, sizeof(e));
    }
    sceIoDclose(dd);
}

static int by_kind_and_title(const void *a, const void *b)
{
    const hm_game *x = a, *y = b;

    return x->kind != y->kind ? x->kind - y->kind : strcmp(x->title, y->title);
}

int hm_library_scan(void)
{
    g_count = 0;
    scan_dir(ISO_DIR, 0);
    scan_apps();
    qsort(g_games, (size_t)g_count, sizeof(g_games[0]), by_kind_and_title);
    play_times();
    save_states();
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

static void load_icon(hm_game *g)
{
    int len, w = 0, h = 0;
    unsigned char *png = load(g->kind, g->path, PART_ICON0, ICON_MAX, &len);
    unsigned int *px = png ? hm_png_decode(png, len, &w, &h) : 0, *icon = 0;

    free(png);
    g->icon_tried = 1;
    if (!px)
        return;
    if ((icon = malloc(HM_ICON_W * HM_ICON_H * 4)) != 0) {
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
        g->icon = icon;
    }
    hm_image_free(px);
}

static int g_next; /* the buffer the next background goes to */

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
        hm_make_background(px, w, h, g_bg[slot]);
        g_bg_game[slot] = sel;
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

        if (sel != tried_bg && sel >= 0 && sel < g_count &&
            (int)(hm_hw_now_ms() - g_focus_ms) >= SETTLE_MS) {
            tried_bg = sel;
            load_background(sel);
            continue;
        }
        for (int i = 0; i < g_count; i++) /* the nearest icon not tried yet */
            if (!g_games[i].icon_tried) {
                int d = i > sel ? i - sel : sel - i;

                if (d < dist) {
                    dist = d;
                    best = i;
                }
            }
        if (best >= 0 && dist < 12)
            load_icon(&g_games[best]);
        else
            sceKernelDelayThread(30 * 1000);
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
