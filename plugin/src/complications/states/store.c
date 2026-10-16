// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "complications/states/engine.h"
#include "complications/states/savedata_hash.h"
#include "complications/states/sfo.h"
#include "complications/states/store.h"
#include "core/crc32.h"
#include "core/game.h"
#include "core/log.h"
#include "core/mem.h"
#include "core/platform.h"
#include "core/png.h"
#include "stb_sprintf.h"
#include "ui/i18n.h"

#define SAVEDATA_DIR "ms0:/PSP/SAVEDATA/"
#define FILE_MAGIC   0x54535750u /* "PWST" */
#define FILE_VERSION 1
#define CHUNK        (1024 * 1024)
#define ICON_PNG_MAX (40 * 1024)
#define FRAME_W      480
#define FRAME_H      272
#define CHECK_ARMED  0x4B435750u /* "PWCK" */
#define NOT_FOUND    ((int)0x80010002)
#define READY_WAIT   3000 /* ms the card may need after a resume */

enum { U_MAGIC, U_SLOT, U_STAMP, U_SUM, U_LEN, U_COUNT };

/* STATE.BIN: everything needed to decide whether a slot can be loaded, written last. */
typedef struct {
    unsigned int magic, version, size;
    unsigned int header_len, image_len, arena_len, data_len;
    unsigned int header_sum;
    pw_engine_env env;
    char game[16];
    char iso[256];
    unsigned int iso_size;
    pw_datetime saved;
    char build[32];
    unsigned int stamp; /* names the marker file created with this save */
    unsigned int reserved[7];
} state_file;

static pw_mem_block g_sfo_block; /* PW_STORE_SLOTS hashed PARAM.SFO images */
static int g_sfo_ok;
static int g_ready;
static int g_held_slot = -1;
static volatile int g_check_failed; /* written in the restored world, after the rollback */

static void folder(char *out, unsigned int cap, int slot)
{
    stbsp_snprintf(out, (int)cap, SAVEDATA_DIR "PW%s%02d", pw_game_get()->id, slot + 1);
}

static void path(char *out, unsigned int cap, int slot, const char *file)
{
    char dir[96];

    folder(dir, sizeof(dir), slot);
    stbsp_snprintf(out, (int)cap, "%s/%s", dir, file);
}

int pw_store_init(void)
{
    const pw_game *g = pw_game_get();
    int ret;

    pw_store_shutdown();
    if (!g->ok) {
        pw_log("store: no game ID: states stay in RAM");
        return PW_STORE_E_NO_GAME;
    }
    g_ready = 1;
    if (pw_mem_alloc_high(&g_sfo_block, PW_MEM_PARTITION_EXTRA, "PwSfo",
                          PW_STORE_SLOTS * PW_SFO_SAVEDATA_SIZE) < 0) {
        pw_log("store: no memory for the PARAM.SFO files: folders without one");
        return 0;
    }
    /* A state's kernel image remembers this block: it must be at the same place to load. */
    pw_engine_set_env_extra(2, (unsigned int)(unsigned long)g_sfo_block.ptr);
    if ((ret = pw_savedata_hash_init()) < 0) {
        pw_log("store: chnnlsv not available (0x%08X): folders without PARAM.SFO", ret);
        return 0;
    }
    g_sfo_ok = 1;
    for (int slot = 0; slot < PW_STORE_SLOTS && g_sfo_ok; slot++) {
        unsigned char *sfo = (unsigned char *)g_sfo_block.ptr + slot * PW_SFO_SAVEDATA_SIZE;
        char dir[32], title[32];
        unsigned int params = 0;
        pw_savedata_sfo s;

        stbsp_snprintf(dir, sizeof(dir), "PW%s%02d", g->id, slot + 1);
        stbsp_snprintf(title, sizeof(title), pw_tr(PW_S_SLOT_TITLE), slot + 1);
        s.title = g->title;
        s.savedata_title = title;
        s.detail = "Pocketwatch";
        s.directory = dir;
        s.file_list = 0;
        s.parental_level = g->parental;
        if (pw_sfo_build_savedata(&s, sfo, PW_SFO_SAVEDATA_SIZE, &params) < 0 ||
            pw_savedata_hash_sfo(sfo, PW_SFO_SAVEDATA_SIZE, params) < 0)
            g_sfo_ok = 0;
    }
    pw_savedata_hash_done();
    pw_log("store: %d slots for %s, PARAM.SFO %s", PW_STORE_SLOTS, g->id,
           g_sfo_ok ? "hashed" : "NOT hashed (the XMB will not list the states)");
    return 0;
}

void pw_store_shutdown(void)
{
    if (g_sfo_block.ptr)
        pw_mem_free(&g_sfo_block);
    g_sfo_block.ptr = 0;
    g_sfo_ok = 0;
    g_ready = 0;
    g_held_slot = -1;
}

int pw_store_available(void)
{
    return g_ready;
}

int pw_store_held_slot(void)
{
    return pw_engine_has_state() ? g_held_slot : -1;
}

static void marker(char *out, unsigned int cap, int slot, unsigned int stamp)
{
    char name[16];

    stbsp_snprintf(name, sizeof(name), "PW%06X.BIN", stamp & 0xFFFFFF);
    path(out, cap, slot, name);
}

static int read_header(int slot, state_file *h)
{
    char p[128];

    path(p, sizeof(p), slot, "STATE.BIN");
    if (pw_file_read(p, h, sizeof(*h)) != (int)sizeof(*h) || h->magic != FILE_MAGIC ||
        h->version != FILE_VERSION || h->size != sizeof(*h))
        return PW_STORE_E_EMPTY;
    h->game[sizeof(h->game) - 1] = 0; /* a damaged file must not run strings off the end */
    h->iso[sizeof(h->iso) - 1] = 0;
    h->build[sizeof(h->build) - 1] = 0;
    return 0;
}

void pw_store_scan(pw_slot_info *slots, unsigned int *icons)
{
    pw_mem_block mem;
    unsigned char *png = 0;

    if (icons && pw_mem_alloc(&mem, "PwIconPng", ICON_PNG_MAX) >= 0)
        png = mem.ptr;

    for (int i = 0; i < PW_STORE_SLOTS; i++) {
        pw_slot_info *s = &slots[i];
        state_file h;

        memset(s, 0, sizeof(*s));
        s->icon = icons ? icons + i * PW_STORE_ICON_W * PW_STORE_ICON_H : 0;
        if (!g_ready || read_header(i, &h) < 0 || strcmp(h.game, pw_game_get()->id) != 0)
            continue;
        s->exists = 1;
        s->saved = h.saved;
        s->size = h.data_len;
        if (s->icon && png) {
            char p[128];
            unsigned int w = 0, hh = 0;
            int n;

            path(p, sizeof(p), i, "ICON0.PNG");
            n = pw_file_read(p, png, ICON_PNG_MAX);
            s->has_icon = n > 0 &&
                          pw_png_decode_stored(png, (unsigned int)n, s->icon,
                                               PW_STORE_ICON_W * PW_STORE_ICON_H, &w, &hh) == 0 &&
                          w == PW_STORE_ICON_W && hh == PW_STORE_ICON_H;
        }
    }
    if (png)
        pw_mem_free(&mem);
}

/* ---- writing ---- */

typedef struct {
    const unsigned int *frame;
    int stride;
} frame_src;

/* ICON0 144x80: the frame scaled by 0.3 (area average), 1 px cut at top and bottom. */
static void icon_row(void *ctx, unsigned int y, unsigned char *rgb)
{
    const frame_src *f = ctx;

    for (unsigned int x = 0; x < PW_STORE_ICON_W; x++) {
        unsigned int sx = x * FRAME_W / PW_STORE_ICON_W, sy = (y * FRAME_H) / 82 + 3, r = 0, g = 0,
                     b = 0, n = 0;

        for (unsigned int dy = 0; dy < 3 && sy + dy < FRAME_H; dy++)
            for (unsigned int dx = 0; dx < 3 && sx + dx < FRAME_W; dx++) {
                unsigned int p = f->frame[(sy + dy) * (unsigned int)f->stride + sx + dx];

                r += p & 0xFF;
                g += (p >> 8) & 0xFF;
                b += (p >> 16) & 0xFF;
                n++;
            }
        rgb[3 * x] = (unsigned char)(r / n);
        rgb[3 * x + 1] = (unsigned char)(g / n);
        rgb[3 * x + 2] = (unsigned char)(b / n);
    }
}

static void pic_row(void *ctx, unsigned int y, unsigned char *rgb)
{
    const frame_src *f = ctx;
    const unsigned int *row = f->frame + y * (unsigned int)f->stride;

    for (unsigned int x = 0; x < FRAME_W; x++) {
        rgb[3 * x] = (unsigned char)(row[x] & 0xFF);
        rgb[3 * x + 1] = (unsigned char)((row[x] >> 8) & 0xFF);
        rgb[3 * x + 2] = (unsigned char)((row[x] >> 16) & 0xFF);
    }
}

static int write_file(const char *p, const void *data, unsigned int len)
{
    return pw_file_write(p, data, len, 0) == (int)len ? 0 : PW_STORE_E_IO;
}

/* Append a part of DATA.BIN in 1 MB pieces, reporting progress against the total. */
static int write_part(int fd, const void *data, unsigned int len, unsigned int *done,
                      unsigned int total, pw_store_progress cb, void *ctx)
{
    const unsigned char *p = data;

    while (len > 0) {
        unsigned int n = len < CHUNK ? len : CHUNK;

        if (pw_file_put(fd, p, n) != (int)n)
            return PW_STORE_E_IO;
        p += n;
        len -= n;
        *done += n;
        if (cb)
            cb((int)((unsigned long long)*done * 100 / total), ctx);
    }
    return 0;
}

static int write_pngs(int slot, const unsigned int *frame, int stride)
{
    unsigned int icon_cap = pw_png_size(PW_STORE_ICON_W, PW_STORE_ICON_H);
    unsigned int pic_cap = pw_png_size(FRAME_W, FRAME_H);
    frame_src src = {frame, stride};
    pw_mem_block mem;
    char p[128];
    int n, ret = 0;

    if (frame == 0)
        return 0;
    if (pw_mem_alloc(&mem, "PwPng", pic_cap + 1 + 3 * FRAME_W + 64) < 0)
        return PW_STORE_E_MEMORY;
    unsigned char *out = mem.ptr, *scratch = out + pic_cap;

    if ((n = pw_png_encode_rgb(PW_STORE_ICON_W, PW_STORE_ICON_H, icon_row, &src, scratch, out,
                               icon_cap)) > 0) {
        path(p, sizeof(p), slot, "ICON0.PNG");
        ret = write_file(p, out, (unsigned int)n);
    }
    if (ret == 0 &&
        (n = pw_png_encode_rgb(FRAME_W, FRAME_H, pic_row, &src, scratch, out, pic_cap)) > 0) {
        path(p, sizeof(p), slot, "PIC1.PNG");
        ret = write_file(p, out, (unsigned int)n);
    }
    pw_mem_free(&mem);
    return ret;
}

int pw_store_save(int slot, const unsigned int *frame, int stride, pw_store_progress cb, void *ctx)
{
    const pw_game *g = pw_game_get();
    pw_state_parts parts;
    state_file h;
    char dir[96], p[128];
    unsigned int done = 0, total;
    int fd, ret;

    if (!g_ready)
        return PW_STORE_E_NO_GAME;
    if (pw_log_card_blocked())
        return PW_STORE_E_BLOCKED;
    if (slot < 0 || slot >= PW_STORE_SLOTS || pw_engine_export(&parts) < 0)
        return PW_STORE_E_NO_STATE;
    total = parts.header_len + parts.image_len + parts.arena_len;
    if (read_header(slot, &h) == 0 && h.stamp) { /* the old state's marker goes first */
        marker(p, sizeof(p), slot, h.stamp);
        pw_file_remove(p);
    }
    folder(dir, sizeof(dir), slot);
    pw_dir_make("ms0:/PSP");
    pw_dir_make("ms0:/PSP/SAVEDATA");
    pw_dir_make(dir); /* all three usually exist */

    /* The slot is empty until the very end: an interrupted save never looks like a state. */
    memset(&h, 0, sizeof(h));
    path(p, sizeof(p), slot, "STATE.BIN");
    if (write_file(p, &h, sizeof(h)) < 0) {
        pw_log("store: cannot write %s", p);
        return PW_STORE_E_IO;
    }

    path(p, sizeof(p), slot, "DATA.BIN");
    fd = pw_file_open(p, 1);
    if (fd < 0)
        return PW_STORE_E_IO;
    ret = write_part(fd, parts.header, parts.header_len, &done, total, cb, ctx);
    if (ret == 0)
        ret = write_part(fd, parts.image, parts.image_len, &done, total, cb, ctx);
    if (ret == 0)
        ret = write_part(fd, parts.arena, parts.arena_len, &done, total, cb, ctx);
    pw_file_close(fd);
    if (ret < 0) {
        pw_log("store: writing %s failed after %u of %u bytes (card full?)", p, done, total);
        return ret;
    }
    if ((ret = write_pngs(slot, frame, stride)) < 0)
        pw_log("store: slot %d: the icon/picture could not be written (%d)", slot + 1, ret);
    if (g_sfo_ok) {
        path(p, sizeof(p), slot, "PARAM.SFO");
        if (write_file(p, (unsigned char *)g_sfo_block.ptr + slot * PW_SFO_SAVEDATA_SIZE,
                       PW_SFO_SAVEDATA_SIZE) < 0)
            pw_log("store: slot %d: PARAM.SFO could not be written", slot + 1);
    }

    h.magic = FILE_MAGIC;
    h.version = FILE_VERSION;
    h.size = sizeof(h);
    h.header_len = parts.header_len;
    h.image_len = parts.image_len;
    h.arena_len = parts.arena_len;
    h.data_len = total;
    h.header_sum = pw_sum32(parts.header, parts.header_len);
    pw_engine_env_now(&h.env);
    strncpy(h.game, g->id, sizeof(h.game) - 1);
    strncpy(h.iso, g->iso, sizeof(h.iso) - 1);
    h.iso_size = g->iso_size;
    pw_local_time(&h.saved);
    strncpy(h.build, PW_ENGINE_BUILD, sizeof(h.build) - 1);
    {
        unsigned int seed[3] = {pw_time_ms(), h.header_sum, total};

        h.stamp = (pw_crc32(pw_crc32(0, (const unsigned char *)&h.saved, sizeof(h.saved)),
                            (const unsigned char *)seed, sizeof(seed)) &
                   0xFFFFFF) |
                  1;
    }
    marker(p, sizeof(p), slot, h.stamp);
    if (write_file(p, &h.stamp, sizeof(h.stamp)) < 0)
        return PW_STORE_E_IO;
    path(p, sizeof(p), slot, "STATE.BIN");
    if (write_file(p, &h, sizeof(h)) < 0)
        return PW_STORE_E_IO;
    g_held_slot = slot;
    pw_log("store: state written to slot %d (%u KB)", slot + 1, total / 1024);
    return 0;
}

/* ---- reading ---- */

static int read_part(int fd, void *data, unsigned int len, unsigned int *done, unsigned int total,
                     pw_store_progress cb, void *ctx)
{
    unsigned char *p = data;

    while (len > 0) {
        unsigned int n = len < CHUNK ? len : CHUNK;

        if (pw_file_get(fd, p, n) != (int)n)
            return PW_STORE_E_IO;
        p += n;
        len -= n;
        *done += n;
        if (cb)
            cb((int)((unsigned long long)*done * 100 / total), ctx);
    }
    return 0;
}

int pw_store_load(int slot, pw_store_progress cb, void *ctx)
{
    const pw_game *g = pw_game_get();
    pw_state_parts parts;
    pw_engine_env now;
    state_file h;
    char p[128];
    unsigned int done = 0;
    int fd, ret;

    if (!g_ready)
        return PW_STORE_E_NO_GAME;
    if (slot < 0 || slot >= PW_STORE_SLOTS || read_header(slot, &h) < 0)
        return PW_STORE_E_EMPTY;
    if (slot == pw_store_held_slot()) {
        pw_log("store: slot %d is the state held in RAM: no read needed", slot + 1);
        return 0;
    }
    if (strcmp(h.game, g->id) != 0 || (g->iso[0] && h.iso[0] && strcmp(h.iso, g->iso) != 0) ||
        (g->iso_size && h.iso_size && h.iso_size != g->iso_size)) {
        pw_log("store: slot %d belongs to %s (%s), this is %s (%s)", slot + 1, h.game, h.iso, g->id,
               g->iso);
        return PW_STORE_E_OTHER_GAME;
    }
    pw_engine_env_now(&now);
    if (strncmp(h.build, PW_ENGINE_BUILD, sizeof(h.build)) != 0 ||
        memcmp(&h.env, &now, sizeof(now)) != 0) { /* before the state in RAM is dropped */
        pw_log("store: slot %d was saved in another setup (build %s, %08X/%08X)", slot + 1, h.build,
               h.env.build, now.build);
        return PW_STORE_E_OTHER_SETUP;
    }
    if ((ret = pw_engine_import(&parts)) < 0)
        return ret == PW_ENGINE_BUSY ? ret : PW_STORE_E_MEMORY;
    g_held_slot = -1;
    if (h.header_len != parts.header_len || h.image_len != parts.image_len ||
        h.arena_len > parts.arena_len)
        return PW_STORE_E_OTHER_SETUP;

    path(p, sizeof(p), slot, "DATA.BIN");
    fd = pw_file_open(p, 0);
    if (fd < 0)
        return PW_STORE_E_IO;
    ret = read_part(fd, parts.header, h.header_len, &done, h.data_len, cb, ctx);
    if (ret == 0)
        ret = read_part(fd, parts.image, h.image_len, &done, h.data_len, cb, ctx);
    if (ret == 0)
        ret = read_part(fd, parts.arena, h.arena_len, &done, h.data_len, cb, ctx);
    pw_file_close(fd);
    if (ret < 0)
        return ret;
    if (pw_sum32(parts.header, h.header_len) != h.header_sum)
        return PW_STORE_E_DAMAGED;
    ret = pw_engine_adopt();
    if (ret == PW_ENGINE_E_ENV)
        return PW_STORE_E_OTHER_SETUP;
    if (ret < 0)
        return PW_STORE_E_DAMAGED;
    g_held_slot = slot;
    pw_log("store: slot %d read (%u KB), ready to load", slot + 1, h.data_len / 1024);
    return 0;
}

int pw_store_delete(int slot)
{
    static const char *const files[] = {"STATE.BIN", "DATA.BIN", "ICON0.PNG", "PIC1.PNG",
                                        "PARAM.SFO"};
    char p[128];
    state_file h;

    if (!g_ready || slot < 0 || slot >= PW_STORE_SLOTS)
        return PW_STORE_E_EMPTY;
    if (pw_log_card_blocked())
        return PW_STORE_E_BLOCKED;
    if (read_header(slot, &h) == 0 && h.stamp) {
        marker(p, sizeof(p), slot, h.stamp);
        pw_file_remove(p);
    }
    for (unsigned int i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        path(p, sizeof(p), slot, files[i]);
        pw_file_remove(p);
    }
    folder(p, sizeof(p), slot);
    pw_dir_remove(p);
    if (g_held_slot == slot)
        g_held_slot = -1;
    pw_log("store: slot %d deleted", slot + 1);
    return 0;
}

const char *pw_store_reason(int err)
{
    switch (err) {
    case PW_STORE_E_NO_GAME:
        return pw_tr(PW_S_E_NO_GAME);
    case PW_STORE_E_IO:
        return pw_tr(PW_S_E_IO);
    case PW_STORE_E_EMPTY:
        return pw_tr(PW_S_E_EMPTY);
    case PW_STORE_E_OTHER_GAME:
        return pw_tr(PW_S_E_OTHER_GAME);
    case PW_STORE_E_OTHER_SETUP:
        return pw_tr(PW_S_E_OTHER_SETUP);
    case PW_STORE_E_DAMAGED:
        return pw_tr(PW_S_E_DAMAGED);
    case PW_STORE_E_NO_STATE:
        return pw_tr(PW_S_E_NO_STATE);
    case PW_STORE_E_MEMORY:
        return pw_tr(PW_S_E_MEMORY);
    case PW_STORE_E_BLOCKED:
        return pw_tr(PW_S_E_BLOCKED);
    case PW_ENGINE_BUSY:
        return pw_tr(PW_S_BUSY);
    default:
        return pw_tr(PW_S_DETAILS_LOG);
    }
}

/* ---- the check after a load ---- */

void pw_store_arm(int slot)
{
    unsigned int *user = pw_engine_user_area();
    state_file h;

    if (user == 0)
        return;
    user[U_MAGIC] = 0;
    if (slot < 0 || read_header(slot, &h) < 0 || h.stamp == 0)
        return;
    user[U_SLOT] = (unsigned int)slot;
    user[U_STAMP] = h.stamp;
    user[U_SUM] = h.header_sum;
    user[U_LEN] = h.data_len;
    user[U_MAGIC] = CHECK_ARMED;
}

/* In the restored world: the game is frozen, nothing has written to the card yet. */
int pw_store_after_load(unsigned int *user, unsigned int words)
{
    unsigned int t0 = pw_time_ms();
    int slot, n, ok;
    char p[128];
    state_file h;

    g_check_failed = 0;
    if (words < U_COUNT || user[U_MAGIC] != CHECK_ARMED)
        return 0;
    user[U_MAGIC] = 0;
    slot = (int)user[U_SLOT];
    if (slot < 0 || slot >= PW_STORE_SLOTS)
        return 0;

    /* ARK's mscache keeps one 4 KB window of msstor reads; any other msstor call drops it
       (docs/research/m3-menu-e3.md, T1). A getstat on the raw device is harmless. */
    pw_file_size("msstor0p1:/");
    marker(p, sizeof(p), slot, user[U_STAMP]);
    for (;;) { /* the card may still be coming back from the suspend */
        n = pw_file_size(p);
        if (n >= 0 || n == NOT_FOUND || pw_time_ms() - t0 > READY_WAIT)
            break;
        pw_delay_ms(50);
    }
    ok = n == (int)sizeof(unsigned int);
    if (ok)
        ok = read_header(slot, &h) == 0 && h.stamp == user[U_STAMP] &&
             h.header_sum == user[U_SUM] && h.data_len == user[U_LEN];
    if (ok) {
        path(p, sizeof(p), slot, "DATA.BIN");
        ok = pw_file_size(p) == (int)user[U_LEN];
    }
    pw_log("store: after the load: the card %s (marker %06X, %u ms)",
           ok ? "is current" : "LOOKS STALE: card writes stop until reboot", user[U_STAMP],
           pw_time_ms() - t0);
    g_check_failed = !ok;
    return ok ? 0 : -1;
}

int pw_store_check_failed(void)
{
    return g_check_failed;
}
