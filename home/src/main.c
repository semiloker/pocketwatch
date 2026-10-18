// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Pocketwatch Home (docs/subsystems/home.md, ADR-025): a home screen of our own - the games on
 * the card with their art, play time and save states, the battery and the card - drawn by
 * Pocketwatch's renderer in the Modern style. A homebrew application: started from the XMB (Game
 * -> Memory Stick), or - XMB -> Settings -> Pocketwatch -> Start screen - by ARK-4 instead of the
 * XMB (ADR-026). Games start the way ARK-4's own menu starts them; Exit to the XMB and HOME ->
 * Quit leave a marker so that the next start shows the XMB, not Home again.
 */
#include <malloc.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <psppower.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/pgf.h"
#include "gfx/text.h"
#include "gfx_gu.h"
#include "home_view.h"
#include "hw.h"
#include "launch.h"
#include "library.h"
#include "stb_sprintf.h"
#include "texts.h"
#include "ui/anim.h"
#include "ui/i18n.h"
#include "ui/icons.h"
#include "ui/input.h"

PSP_MODULE_INFO("PocketwatchHome", PSP_MODULE_USER, 0, 1);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-2048);

#define FONT_PATH "flash0:/font/ltn0.pgf"
#define PW_DIR    "ms0:/seplugins/pocketwatch/"
#define XMB_ONCE  PW_DIR "xmb-once"     /* read by the XMB plugin (vsh/src/xboot.c) */
#define PENDING   PW_DIR "boot-pending" /* the XMB plugin's crash guard: Home did come up */
#define HOME_LOG  PW_DIR "home.log"     /* the frame rate, every STATS_MS */
#define STATS_MS  30000
#define FADE_MS   250
#define MIX_MS    300 /* a new background fades in */
#define ROWS_MAX  (HM_GAMES_MAX + 8)
#define REPEAT    (PSP_CTRL_LEFT | PSP_CTRL_RIGHT | PSP_CTRL_UP | PSP_CTRL_DOWN)

static hm_ui g_ui;
static pw_font g_fonts[3];
static hm_tile g_tiles[HM_GAMES_MAX];
static hm_row g_rows[ROWS_MAX];
static char g_vals[ROWS_MAX][48];
static unsigned int *g_gradient;
static unsigned int g_free_mb; /* read when the System section opens, not every second */

/* ---- fonts: the system's own (flash0), else the built-in 8x8 ---- */

static void load_fonts(void)
{
    static pw_pgf pgf;
    static const int scales[3] = {205, 160,
                                  256}; /* title ~14 px, body ~11 px, big: the native ~18 px */
    static const unsigned int pools[3] = {40 * 1024, 28 * 1024, 72 * 1024};
    SceUID fd = sceIoOpen(FONT_PATH, PSP_O_RDONLY, 0);
    int size = fd >= 0 ? (int)sceIoLseek32(fd, 0, PSP_SEEK_END) : -1, ok = 0;
    unsigned char *file = size > 0 ? malloc((size_t)size) : 0;

    if (file) {
        sceIoLseek32(fd, 0, PSP_SEEK_SET);
        ok = sceIoRead(fd, file, (unsigned int)size) == size &&
             pw_pgf_open(&pgf, file, (unsigned int)size) == 0;
    }
    if (fd >= 0)
        sceIoClose(fd);
    for (int i = 0; i < 3 && ok; i++) {
        unsigned int scratch = pw_font_scratch_size(&pgf);
        void *pool = malloc(pools[i]), *scr = malloc(scratch);

        ok = pool && scr &&
             pw_font_init(&g_fonts[i], &pgf, scales[i], pool, pools[i], scr, scratch) == 0;
    }
    if (!ok) {
        pw_font_init_fallback(&g_fonts[0], 2);
        pw_font_init_fallback(&g_fonts[1], 1);
        pw_font_init_fallback(&g_fonts[2], 2);
    }
    g_ui.title = &g_fonts[0];
    g_ui.body = &g_fonts[1];
    g_ui.big = &g_fonts[2];
}

/* ---- texts ---- */

static void duration(char *out, int cap, unsigned int s)
{
    unsigned int m = s / 60;

    if (m >= 60)
        stbsp_snprintf(out, cap, pw_tr(PW_S_DUR_HM), (int)(m / 60), (int)(m % 60));
    else
        stbsp_snprintf(out, cap, pw_tr(PW_S_DUR_M), (int)m);
}

/* Each game's line under its title: disc ID, play time, save states - what is known. */
static void build_tiles(void)
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
        if (g->play_s >= 60) {
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
        g_tiles[i].title = g->title;
        g_tiles[i].meta = g->meta[0] ? g->meta : 0;
    }
}

/* Play Time: the total, then every game that has some, most first, with a bar. */
static int playtime_rows(void)
{
    int order[HM_GAMES_MAX], n = 0, rows = 0;
    unsigned int total = 0, most = 1;

    for (int i = 0; i < hm_library_count(); i++)
        if (hm_library_game(i)->play_s >= 60) {
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
    duration(g_vals[rows], 48, total);
    g_rows[rows++] = (hm_row){pw_tr(PW_S_PT_TOTAL), n ? g_vals[0] : pw_tr(PW_S_PT_NONE), -1};
    for (int i = 0; i < n && rows < ROWS_MAX - 1; i++) {
        hm_game *g = hm_library_game(order[i]);

        duration(g_vals[rows], 48, g->play_s);
        g_rows[rows] = (hm_row){g->title, g_vals[rows], (int)(g->play_s * 256ull / most)};
        rows++;
    }
    g_rows[rows++] = (hm_row){pw_tr(PW_S_PT_NOTE), 0, -1};
    return rows;
}

/* System: battery, time left, the card, the version, and the way back to the XMB. */
static int system_rows(int *exit_row)
{
    int rows = 0, charging = 0, pct = hm_hw_battery(&charging), left = scePowerGetBatteryLifeTime();

    if (pct >= 0)
        stbsp_snprintf(g_vals[rows], 48, "%d%%   %s", pct,
                       pw_tr(charging ? PW_S_BAT_CHARGING : PW_S_BAT_ON_BATTERY));
    g_rows[rows] = (hm_row){pw_tr(PW_S_BATTERY), pct >= 0 ? g_vals[rows] : pw_tr(PW_S_BAT_NA), -1};
    rows++;
    if (!charging && left > 0 && left < 24 * 60) {
        char d[32];

        duration(d, sizeof(d), (unsigned int)left * 60);
        stbsp_snprintf(g_vals[rows], 48, pw_tr(PW_S_BAT_SYSTEM), d);
        g_rows[rows] = (hm_row){pw_tr(PW_S_BAT_LEFT), g_vals[rows], -1};
        rows++;
    }
    stbsp_snprintf(g_vals[rows], 48, "%u MB", g_free_mb);
    g_rows[rows] = (hm_row){pw_tr(PW_S_HM_FREE), g_vals[rows], -1};
    rows++;
    stbsp_snprintf(g_vals[rows], 48, "%d", hm_library_count());
    g_rows[rows] = (hm_row){pw_tr(PW_S_HM_INSTALLED), g_vals[rows], -1};
    rows++;
    g_rows[rows] = (hm_row){pw_tr(PW_S_HM_VERSION), "Pocketwatch Home " PW_VERSION, -1};
    rows++;
    *exit_row = rows;
    g_rows[rows++] = (hm_row){pw_tr(PW_S_HM_EXIT), 0, -1};
    return rows;
}

/* ---- the frame rate (HOM-NFR-001): home.log, so a slow screen shows in numbers ---- */

typedef struct {
    unsigned int t0, frames, cpu_us, ge_us, worst_ms, last;
} stats;

static void stats_frame(stats *st, unsigned int now, unsigned int cpu_us, unsigned int ge_us)
{
    if (st->frames && now - st->last > st->worst_ms)
        st->worst_ms = now - st->last;
    st->last = now;
    st->frames++;
    st->cpu_us += cpu_us;
    st->ge_us += ge_us;
}

static void stats_write(stats *st, unsigned int now, const char *why)
{
    char line[160];
    unsigned int ms = now - st->t0, n = st->frames ? st->frames : 1;
    SceUID fd;

    if (st->frames < 2 || ms == 0)
        return;
    stbsp_snprintf(line, sizeof(line),
                   "home %s: %u frames in %u ms (%u fps), CPU %u us + GE %u us a frame, "
                   "longest frame %u ms\n",
                   why, st->frames, ms, st->frames * 1000 / ms, st->cpu_us / n, st->ge_us / n,
                   st->worst_ms);
    fd = sceIoOpen(HOME_LOG, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, line, strlen(line));
        sceIoClose(fd);
    }
    memset(st, 0, sizeof(*st));
    st->t0 = st->last = hm_hw_now_ms();
}

/* ---- the HOME panel (HOM-REQ-010) ---- */

enum { PANEL_CLOSE, PANEL_XMB, PANEL_SLEEP, PANEL_OFF, PANEL_ITEMS };

static void panel_texts(hm_view *v)
{
    v->panel_title = "Pocketwatch";
    v->panel_items[PANEL_CLOSE] = pw_tr(PW_S_HM_CLOSE);
    v->panel_items[PANEL_XMB] = pw_tr(PW_S_HM_EXIT);
    v->panel_items[PANEL_SLEEP] = pw_tr(PW_S_HM_SLEEP);
    v->panel_items[PANEL_OFF] = pw_tr(PW_S_HM_POWER_OFF);
    v->panel_count = PANEL_ITEMS;
}

/* ---- the loop ---- */

int main(void)
{
    static hm_view v;
    pw_keys keys = {0, 0, 0};
    stats st;
    unsigned int t_prev, t_open, t_fade = 0, t_rows = 0, t_status = 0, t_free = 0;
    const unsigned int *shown = 0; /* the background drawn last */
    int launching = -1, exit_row = -1, row_sel = 0, up = 0, panel_open = 0, leave = 0;
    unsigned int enter, back;
    char clock[24] = "";

    if (hm_hw_init() < 0)
        return 0;
    pw_icon_cache(malloc(64 * 1024), 64 * 1024);
    load_fonts();
    g_ui.cross_enter = hm_hw_cross_enter();
    enter = g_ui.cross_enter ? PSP_CTRL_CROSS : PSP_CTRL_CIRCLE;
    back = g_ui.cross_enter ? PSP_CTRL_CIRCLE : PSP_CTRL_CROSS;
    hm_texts_load();
    g_gradient = memalign(64, 512 * HM_H * 4);
    if (g_gradient) {
        hm_make_gradient(g_gradient);
        hm_gu_written(g_gradient, 512 * HM_H * 4);
    }
    hm_library_scan();
    build_tiles();
    hm_library_start();

    v.tab_names[HM_TAB_GAMES] = pw_tr(PW_S_HM_GAMES);
    v.tab_names[HM_TAB_PLAYTIME] = pw_tr(PW_S_PLAYTIME);
    v.tab_names[HM_TAB_SYSTEM] = pw_tr(PW_S_HM_SYSTEM);
    v.tiles = g_tiles;
    v.count = hm_library_count();
    v.play_label = pw_tr(PW_S_HM_PLAY);
    v.empty = pw_tr(PW_S_HM_NO_GAMES);
    v.hint_tabs = pw_tr(PW_S_HM_SECTION);
    v.clock = clock;
    v.bg = g_gradient;
    v.bg_mix = 256;
    panel_texts(&v);
    shown = g_gradient;
    t_open = t_prev = hm_hw_now_ms();
    memset(&st, 0, sizeof(st));
    st.t0 = st.last = t_open;

    while (!hm_hw_exit_requested() && !leave) {
        unsigned int now = hm_hw_now_ms(), t_cpu = sceKernelGetSystemTimeLow(),
                     ev = pw_keys_update(&keys, hm_hw_buttons(), REPEAT, now * 1000);
        const unsigned int *want;
        int dt = (int)(now - t_prev);

        t_prev = now;
        /* input: the HOME panel first, over everything */
        if (launching < 0 && (ev & PSP_CTRL_HOME)) {
            panel_open = !panel_open;
            v.panel_sel = 0;
        } else if (launching < 0 && panel_open) {
            if (ev & PSP_CTRL_UP)
                v.panel_sel = v.panel_sel > 0 ? v.panel_sel - 1 : 0;
            else if (ev & PSP_CTRL_DOWN)
                v.panel_sel = v.panel_sel < PANEL_ITEMS - 1 ? v.panel_sel + 1 : v.panel_sel;
            else if (ev & back)
                panel_open = 0;
            else if (ev & enter) {
                panel_open = 0;
                if (v.panel_sel == PANEL_XMB)
                    leave = 1;
                else if (v.panel_sel == PANEL_SLEEP)
                    scePowerRequestSuspend();
                else if (v.panel_sel == PANEL_OFF)
                    scePowerRequestStandby();
            }
        } else if (launching < 0) {
            if (ev & PSP_CTRL_LTRIGGER)
                v.tab = (v.tab + HM_TABS - 1) % HM_TABS;
            else if (ev & PSP_CTRL_RTRIGGER)
                v.tab = (v.tab + 1) % HM_TABS;
            if (ev & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) {
                row_sel = 0;
                v.row_pos = 0;
                t_rows = 0;
            }
            if (v.tab == HM_TAB_GAMES && v.count > 0) {
                if (ev & PSP_CTRL_LEFT)
                    v.sel = v.sel > 0 ? v.sel - 1 : 0;
                else if (ev & PSP_CTRL_RIGHT)
                    v.sel = v.sel < v.count - 1 ? v.sel + 1 : v.sel;
                else if (ev & enter) {
                    launching = v.sel;
                    t_fade = now;
                }
            } else if (v.tab != HM_TAB_GAMES) {
                if (ev & PSP_CTRL_UP)
                    row_sel = row_sel > 0 ? row_sel - 1 : 0;
                else if (ev & PSP_CTRL_DOWN)
                    row_sel = row_sel < v.nrows - 1 ? row_sel + 1 : row_sel;
                else if ((ev & enter) && v.tab == HM_TAB_SYSTEM && row_sel == exit_row)
                    leave = 1;
            }
        }
        /* what changes by the second: the clock, the battery, the cards' rows */
        if (t_status == 0 || now - t_status >= 1000) {
            t_status = now;
            hm_hw_clock(clock, sizeof(clock));
            v.battery = hm_hw_battery(&v.charging);
        }
        if (v.tab == HM_TAB_SYSTEM && (t_free == 0 || now - t_free >= 10000)) {
            t_free = now;
            g_free_mb = hm_hw_free_mb();
        }
        if (v.tab != HM_TAB_GAMES && (t_rows == 0 || now - t_rows >= 1000)) {
            t_rows = now;
            v.nrows = v.tab == HM_TAB_PLAYTIME ? playtime_rows() : system_rows(&exit_row);
            v.rows = g_rows;
            if (row_sel >= v.nrows)
                row_sel = v.nrows - 1;
        }
        v.row_sel = row_sel;
        /* motion, by time */
        v.selpos = pw_ease(v.selpos, v.sel * 256, dt, PW_ANIM_TAU_MS, 6);
        v.tab_pos = pw_ease(v.tab_pos, v.tab * 256, dt, PW_ANIM_TAU_MS, 6);
        v.row_pos = pw_ease(v.row_pos, row_sel * 256, dt, PW_ANIM_TAU_MS, 6);
        v.panel = pw_ease(v.panel, panel_open ? 256 : 0, dt, PW_ANIM_TAU_MS, 6);
        v.panel_pos = pw_ease(v.panel_pos, v.panel_sel * 256, dt, PW_ANIM_TAU_MS, 6);
        hm_library_focus(v.sel);
        for (int i = 0; i < v.count; i++)
            g_tiles[i].icon = hm_library_game(i)->icon;
        /* the chosen game's art, faded in once the loader has it */
        want = v.count ? hm_library_background(v.sel) : 0;
        if (!want)
            want = g_gradient;
        if (want != shown && want) {
            v.bg_prev = shown;
            v.bg = want;
            v.bg_mix = 0;
            shown = want;
        }
        v.bg_mix = v.bg_mix + dt * 256 / MIX_MS > 256 ? 256 : v.bg_mix + dt * 256 / MIX_MS;
        v.hint_enter = v.tab == HM_TAB_GAMES && v.count ? v.play_label : 0;
        v.t_ms = now;
        v.appear =
            launching >= 0 ? 256 - pw_ramp(now, t_fade, FADE_MS) : pw_ramp(now, t_open, FADE_MS);

        hm_draw(hm_gu_begin(), &g_ui, &v);
        t_cpu = sceKernelGetSystemTimeLow() - t_cpu;
        stats_frame(&st, now, t_cpu, hm_gu_end());
        if (!up) { /* the first screen is up: tell the XMB plugin's guard */
            hm_hw_remove(PENDING);
            up = 1;
        }
        if (now - st.t0 >= STATS_MS)
            stats_write(&st, now, "running");

        if (launching >= 0 && now - t_fade >= FADE_MS) {
            hm_game *g = hm_library_game(launching);

            stats_write(&st, now, "before a game");
            if (g->kind == HM_KIND_ISO)
                hm_launch(g->path, hm_library_has(g->path, "PSP_GAME/SYSDIR/EBOOT.OLD"));
            else
                hm_launch_eboot(g->path, g->kind == HM_KIND_PS1);
            launching = -1; /* it returned: the start failed; stay */
            t_open = now;
        }
    }
    stats_write(&st, hm_hw_now_ms(), "leaving");
    hm_hw_touch(XMB_ONCE); /* leaving Home means the XMB, even when Home is the start screen */
    sceKernelExitGame();
    return 0;
}
