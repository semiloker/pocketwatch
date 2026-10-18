// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * The Play Time complication (PLT-REQ-001..006, docs/subsystems/playtime.md): it counts how long
 * the game is played in this boot and keeps every game's totals in playtime-a.txt /
 * playtime-b.txt next to the PRX.
 *
 * What counts: the shell's once-a-second tick while the game runs and the hub is closed, each
 * step at most 2.5 s - so the hub, saves and loads (no tick while busy) and sleeps (the step after
 * a resume is long) are not play time. The session's counters live in a small P9 block, which a
 * state's rollback does not touch (the state holds kernel RAM, not P9): a load keeps the time
 * played since the save. The table of all games is only in memory (P11) while it is written. The
 * page is in the XMB (Game -> Play Time, vsh/; ADR-024).
 */
#include <string.h>

#include "complications/complication.h"
#include "complications/states/engine.h"
#include "core/game.h"
#include "core/log.h"
#include "core/mem.h"
#include "core/platform.h"
#include "core/playtime.h"
#include "stb_sprintf.h"

#define LIVE_MAGIC   0x50574C54u /* "PWLT" */
#define STEP_MAX_MS  2500        /* a longer step was not play (a sleep, the hub, a save) */
#define SAVE_EVERY_S 300         /* write at least this often while playing */

typedef struct {
    unsigned int magic;
    unsigned int played_ms; /* this boot's play */
    unsigned int saved_s;   /* what the files already have of it */
    unsigned int last_tick; /* ms of the last tick */
    int file;               /* which of the two files has the newest copy (0 a, 1 b), -1 none */
    unsigned int seq;       /* its sequence number */
    pw_pt_base base;        /* the game's record when this boot began */
    pw_datetime start;      /* when this boot's session began */
    int ready;
    int want_save; /* the hub was open: write at the next tick (not while it opens) */
} live;

static pw_mem_block g_live_mem;
static live *g_live;

static void file_path(char *out, unsigned int cap, int which)
{
    char dir[200];

    if (pw_log_dir(dir, sizeof(dir)) < 0)
        strcpy(dir, "ms0:/seplugins/pocketwatch/");
    stbsp_snprintf(out, (int)cap, "%splaytime-%c.txt", dir, which ? 'b' : 'a');
}

/* Parse one file into t; returns its sequence number + 1, or 0 when it is missing or broken. */
static unsigned int parse_file(pw_pt_table *t, char *text, int which)
{
    char path[256];
    int n;

    file_path(path, sizeof(path), which);
    n = pw_file_read(path, text, PW_PT_TEXT_MAX);
    return n > 0 && pw_pt_parse(t, text, (unsigned int)n) >= 0 ? t->seq + 1 : 0;
}

/* The newer complete one of the two files into t (empty when there is none). Returns which, or
   -1. text: PW_PT_TEXT_MAX bytes of scratch. */
static int read_table(pw_pt_table *t, char *text)
{
    unsigned int a = parse_file(t, text, 0), b = parse_file(t, text, 1);

    if (!a && !b) {
        memset(t, 0, sizeof(*t));
        return -1;
    }
    if (b > a) /* t already holds b */
        return 1;
    parse_file(t, text, 0);
    return 0;
}

/* Scratch for the table and its text, from P11 (free while no suspend runs). */
typedef struct {
    pw_pt_table table;
    char text[PW_PT_TEXT_MAX];
} scratch;

static scratch *get_scratch(pw_mem_block *m)
{
    if (pw_mem_alloc_in(m, PW_MEM_PARTITION_SPARE, "PwPlayTimeTmp", sizeof(scratch)) < 0 &&
        pw_mem_alloc(m, "PwPlayTimeTmp", sizeof(scratch)) < 0)
        return 0;
    return m->ptr;
}

/* This game's record with the session so far (into t). */
static pw_pt_game *with_session(pw_pt_table *t)
{
    pw_pt_game *g = pw_pt_find(t, pw_game_get()->id, 1);
    pw_datetime now;

    pw_local_time(&now);
    pw_pt_apply(g, &g_live->base, g_live->played_ms / 1000, &g_live->start, &now,
                pw_game_get()->title);
    return g;
}

/* Write the totals with this session (the other file than the newest, so one stays whole). */
static void save(const char *why)
{
    pw_mem_block m;
    scratch *s;
    char path[256];
    int n, which;

    if (!g_live || !g_live->ready || g_live->played_ms / 1000 == g_live->saved_s ||
        pw_engine_busy())
        return;
    if (!(s = get_scratch(&m)))
        return;
    read_table(&s->table, s->text);
    with_session(&s->table);
    which = g_live->file < 0 ? 0 : !g_live->file;
    s->table.seq = g_live->seq + 1;
    n = pw_pt_format(&s->table, s->text, PW_PT_TEXT_MAX);
    file_path(path, sizeof(path), which);
    if (n > 0 && pw_file_write(path, s->text, (unsigned int)n, 0) == n) {
        g_live->file = which;
        g_live->seq = s->table.seq;
        g_live->saved_s = g_live->played_ms / 1000;
        pw_log("playtime: %u s this session written (%s)", g_live->saved_s, why);
    } else {
        pw_log("playtime: %s could not be written", path);
    }
    pw_mem_free(&m);
}

static void playtime_init(void)
{
    pw_mem_block m;
    scratch *s;

    if (!pw_game_get()->ok ||
        pw_mem_alloc_high(&g_live_mem, PW_MEM_PARTITION_EXTRA, "PwPlayTime", sizeof(live)) < 0)
        return;
    g_live = g_live_mem.ptr;
    /* a state's kernel image remembers this block: it must be at the same place to load */
    pw_engine_set_env_extra(3, (unsigned int)(unsigned long)g_live);
    memset(g_live, 0, sizeof(*g_live));
    g_live->magic = LIVE_MAGIC;
    g_live->file = -1;
    pw_local_time(&g_live->start);
    if ((s = get_scratch(&m))) {
        g_live->file = read_table(&s->table, s->text);
        g_live->seq = s->table.seq;
        pw_pt_base_of(pw_pt_find(&s->table, pw_game_get()->id, 0), &g_live->base);
        pw_mem_free(&m);
    }
    g_live->ready = 1;
    pw_log("playtime: %s %s, %u s so far in %u session(s)", pw_game_get()->id,
           g_live->base.known ? "known" : "new", g_live->base.total_s, g_live->base.sessions);
}

static void playtime_shutdown(void)
{
    g_live = 0; /* module_stop: no I/O; the last minutes since a write are lost */
    pw_mem_free(&g_live_mem);
}

static void playtime_tick(unsigned int now_ms)
{
    unsigned int step;

    if (!g_live || !g_live->ready)
        return;
    step = now_ms - g_live->last_tick;
    if (g_live->last_tick && step <= STEP_MAX_MS)
        g_live->played_ms += step;
    g_live->last_tick = now_ms;
    if (g_live->want_save) {
        g_live->want_save = 0;
        save("after the hub");
    } else if (g_live->played_ms / 1000 >= g_live->saved_s + SAVE_EVERY_S) {
        save("every 5 minutes");
    }
}

static void playtime_open(void)
{
    if (!g_live)
        return;
    g_live->last_tick = 0; /* the hub's time is not play */
    g_live->want_save = 1; /* written once the game runs again: no card write while it opens */
}

static void playtime_quit(void)
{
    save("Quit Game");
}

const pw_complication pw_cpl_playtime = {.id = "playtime",
                                         .init = playtime_init,
                                         .shutdown = playtime_shutdown,
                                         .on_open = playtime_open,
                                         .tick = playtime_tick,
                                         .on_quit = playtime_quit};
