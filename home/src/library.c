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

/* A file of the image into a new buffer (free it); its size in *len. NULL when missing. */
static unsigned char *load(const char *image, const char *inner, unsigned int cap, int *len)
{
    static hm_disc d; /* one at a time per thread: the loader and the scan never overlap */
    SceUID fd = sceIoOpen(image, PSP_O_RDONLY, 0);
    unsigned int off, size;
    unsigned char *buf = 0;

    if (fd < 0)
        return 0;
    if (hm_disc_open(&d, read_fd, hm_inflate_raw, (void *)(long)fd) == 0 &&
        hm_disc_find(&d, inner, &off, &size) == 0 && size > 0 && size <= cap &&
        (buf = malloc(size + 1)) != 0 && hm_disc_read(&d, off, buf, size) != (int)size) {
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

static void add_image(const char *path)
{
    hm_game *g;
    unsigned char *sfo;
    int len;

    if (g_count >= HM_GAMES_MAX)
        return;
    g = &g_games[g_count];
    memset(g, 0, sizeof(*g));
    stbsp_snprintf(g->path, sizeof(g->path), "%s", path);
    if ((sfo = load(path, "PSP_GAME/PARAM.SFO", SFO_MAX, &len)) != 0) {
        sfo_string(sfo, len, "TITLE", g->title, sizeof(g->title));
        sfo_string(sfo, len, "DISC_ID", g->id, sizeof(g->id));
        free(sfo);
    }
    if (!g->title[0]) { /* not a PSP game image: still listed by its file name */
        const char *slash = strrchr(path, '/');

        stbsp_snprintf(g->title, sizeof(g->title), "%s", slash ? slash + 1 : path);
    }
    g_count++;
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
                const pw_pt_game *p = pw_pt_find(t, g_games[i].id, 0);

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

                if (n && strncmp(e.d_name + 2, g_games[i].id, (size_t)n) == 0 &&
                    strlen(e.d_name) == (size_t)n + 4)
                    g_games[i].states++;
            }
        memset(&e, 0, sizeof(e));
    }
    sceIoDclose(dd);
}

static int by_title(const void *a, const void *b)
{
    return strcmp(((const hm_game *)a)->title, ((const hm_game *)b)->title);
}

int hm_library_scan(void)
{
    g_count = 0;
    scan_dir(ISO_DIR, 0);
    qsort(g_games, (size_t)g_count, sizeof(g_games[0]), by_title);
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
    unsigned char *png = load(g->path, "PSP_GAME/ICON0.PNG", ICON_MAX, &len);
    unsigned int *px = png ? hm_png_decode(png, len, &w, &h) : 0, *icon = 0;

    free(png);
    g->icon_tried = 1;
    if (!px)
        return;
    if ((icon = malloc(HM_ICON_W * HM_ICON_H * 4)) != 0) {
        if (w == HM_ICON_W && h == HM_ICON_H)
            memcpy(icon, px, HM_ICON_W * HM_ICON_H * 4);
        else
            hm_image_fit(px, w, h, icon, HM_ICON_W, HM_ICON_H);
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
    unsigned char *png = load(g_games[sel].path, "PSP_GAME/PIC1.PNG", PIC_MAX, &len);
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
