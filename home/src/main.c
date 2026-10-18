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
#include "core/config.h"
#include "dialogs.h"
#include "gfx_gu.h"
#include "home_view.h"
#include "hsettings.h"
#include "hw.h"
#include "launch.h"
#include "library.h"
#include "music.h"
#include "states.h"
#include "stb_sprintf.h"
#include "texts.h"
#include "usb.h"
#include "ui/anim.h"
#include "ui/i18n.h"
#include "ui/icons.h"
#include "ui/input.h"

PSP_MODULE_INFO("PocketwatchHome", PSP_MODULE_USER, 0, 1);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-4096); /* 4 MB left to the firmware's dialogs (keyboard, saved data) */

#define FONT_PATH "flash0:/font/ltn0.pgf"
#define PW_DIR    "ms0:/seplugins/pocketwatch/"
#define CONTINUE  PW_DIR "continue.txt" /* read by the game plugin (M7 V1) */
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
static hm_tile g_ph_tiles[HM_PHOTOS_MAX];
static hm_row g_pt_rows[ROWS_MAX], g_set_rows[32], g_sys_rows[24]; /* Play Time, Settings, System */
static char g_pt_vals[ROWS_MAX][48], g_set_vals[32][48], g_sys_vals[24][48];
static char g_model[24], g_firmware[16], g_cfw[32], g_mac[24]; /* read once */
static hm_row g_mu_rows[HM_TRACKS_MAX + 1];                    /* Music: now playing, the tracks */
static char g_mu_now[32];
static unsigned int *g_gradient;

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
static int playtime_rows(hm_row *out, char (*vals)[48])
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
    duration(vals[rows], 48, total);
    out[rows++] =
        (hm_row){pw_tr(PW_S_PT_TOTAL), n ? vals[0] : pw_tr(PW_S_PT_NONE), -1, HM_ROW_INFO};
    for (int i = 0; i < n && rows < ROWS_MAX - 1; i++) {
        hm_game *g = hm_library_game(order[i]);

        duration(vals[rows], 48, g->play_s);
        out[rows] = (hm_row){g->title, vals[rows], (int)(g->play_s * 256ull / most), HM_ROW_INFO};
        rows++;
    }
    out[rows++] = (hm_row){pw_tr(PW_S_PT_NOTE), 0, -1, HM_ROW_INFO};
    return rows;
}

/* System: battery, time left, the card, the version, USB and the way back to the XMB. */
static int system_rows(hm_row *out, char (*vals)[48], int *usb_row, int *exit_row)
{
    int rows = 0, charging = 0, pct = hm_hw_battery(&charging), left = scePowerGetBatteryLifeTime();

    if (pct >= 0)
        stbsp_snprintf(vals[rows], 48, "%d%%   %s", pct,
                       pw_tr(charging ? PW_S_BAT_CHARGING : PW_S_BAT_ON_BATTERY));
    out[rows] =
        (hm_row){pw_tr(PW_S_BATTERY), pct >= 0 ? vals[rows] : pw_tr(PW_S_BAT_NA), -1, HM_ROW_INFO};
    rows++;
    {
        int mv, temp, rcap, full, cycles;

        hm_hw_battery_details(&mv, &temp, &rcap, &full, &cycles);
        if (mv > 0) {
            stbsp_snprintf(vals[rows], 48, "%d.%02d V", mv / 1000, mv % 1000 / 10);
            out[rows++] = (hm_row){pw_tr(PW_S_BAT_VOLTAGE), vals[rows - 1], -1, HM_ROW_INFO};
        }
        if (temp > 0 && temp < 80) {
            stbsp_snprintf(vals[rows], 48,
                           "%d \xC2\xB0"
                           "C",
                           temp); /* U+00B0 */
            out[rows++] = (hm_row){pw_tr(PW_S_BAT_TEMPERATURE), vals[rows - 1], -1, HM_ROW_INFO};
        }
        if (rcap > 0 && full > 0) {
            stbsp_snprintf(vals[rows], 48, pw_tr(PW_S_BAT_CAPACITY_V), rcap, full);
            out[rows++] = (hm_row){pw_tr(PW_S_BAT_CAPACITY), vals[rows - 1], -1, HM_ROW_INFO};
        }
        if (cycles >= 0) {
            stbsp_snprintf(vals[rows], 48, "%d", cycles);
            out[rows++] = (hm_row){pw_tr(PW_S_BAT_CYCLES), vals[rows - 1], -1, HM_ROW_INFO};
        }
    }
    if (!charging && left > 0 && left < 24 * 60) {
        char d[32];

        duration(d, sizeof(d), (unsigned int)left * 60);
        stbsp_snprintf(vals[rows], 48, pw_tr(PW_S_BAT_SYSTEM), d);
        out[rows] = (hm_row){pw_tr(PW_S_BAT_LEFT), vals[rows], -1, HM_ROW_INFO};
        rows++;
    }
    if (hm_library_free_mb() >= 0)
        stbsp_snprintf(vals[rows], 48, "%d MB", hm_library_free_mb());
    else
        stbsp_snprintf(vals[rows], 48, "...");
    out[rows] = (hm_row){pw_tr(PW_S_HM_FREE), vals[rows], -1, HM_ROW_INFO};
    rows++;
    stbsp_snprintf(vals[rows], 48, "%d", hm_library_count());
    out[rows] = (hm_row){pw_tr(PW_S_HM_INSTALLED), vals[rows], -1, HM_ROW_INFO};
    rows++;
    hs_nickname(vals[rows], 48);
    out[rows] = (hm_row){pw_tr(PW_S_HS_NICKNAME), vals[rows], -1, HM_ROW_INFO};
    rows++;
    out[rows++] = (hm_row){pw_tr(PW_S_HM_MODEL), g_model, -1, HM_ROW_INFO};
    out[rows++] = (hm_row){pw_tr(PW_S_HM_FIRMWARE), g_firmware, -1, HM_ROW_INFO};
    out[rows++] = (hm_row){pw_tr(PW_S_HM_CFW), g_cfw, -1, HM_ROW_INFO};
    if (g_mac[0])
        out[rows++] = (hm_row){pw_tr(PW_S_HM_MAC), g_mac, -1, HM_ROW_INFO};
    out[rows] = (hm_row){pw_tr(PW_S_HM_VERSION), "Pocketwatch Home " PW_VERSION, -1, HM_ROW_INFO};
    rows++;
    *usb_row = rows;
    out[rows++] = (hm_row){pw_tr(PW_S_HM_USB), 0, -1, HM_ROW_ACTION};
    *exit_row = rows;
    out[rows++] = (hm_row){pw_tr(PW_S_HM_EXIT), 0, -1, HM_ROW_ACTION};
    return rows;
}

/* Music (HOM-REQ-018): what plays (time, a bar), then every track - the one playing with a bar. */
static int music_rows(void)
{
    int cur = hm_music_current(), n = hm_music_count(), rows = 0, bar = -1;
    unsigned int played, length;

    hm_music_time(&played, &length);
    if (cur >= 0 && length > 0)
        bar = (int)(played * 256 / length > 256 ? 256 : played * 256 / length);
    if (cur >= 0 && hm_music_paused())
        stbsp_snprintf(g_mu_now, sizeof(g_mu_now), "%s", pw_tr(PW_S_HM_MU_PAUSED));
    else if (cur >= 0 && length > 0)
        stbsp_snprintf(g_mu_now, sizeof(g_mu_now), "%u:%02u / %u:%02u", played / 60, played % 60,
                       length / 60, length % 60);
    else if (cur >= 0)
        stbsp_snprintf(g_mu_now, sizeof(g_mu_now), "%u:%02u", played / 60, played % 60);
    g_mu_rows[rows++] = (hm_row){cur >= 0 ? hm_music_track(cur)->title
                                 : n      ? pw_tr(PW_S_HM_MU_IDLE)
                                          : pw_tr(PW_S_HM_MU_NONE),
                                 cur >= 0 ? g_mu_now : 0, bar, HM_ROW_INFO};
    for (int i = 0; i < n; i++) {
        const hm_track *t = hm_music_track(i);

        g_mu_rows[rows++] =
            (hm_row){t->title, t->artist[0] ? t->artist : 0, i == cur ? bar : -1, HM_ROW_ACTION};
    }
    return rows;
}

/* Settings (HOM-REQ-012): hsettings.c's rows as the card shows them. */
static int settings_rows(hm_row *out, char (*vals)[48])
{
    static const int kinds[3] = {HM_ROW_HEADER, HM_ROW_CHOICE, HM_ROW_ACTION};
    int n = hs_count() < 32 ? hs_count() : 32;

    for (int i = 0; i < n; i++) {
        hs_value(i, vals[i], 48);
        out[i] = (hm_row){hs_label(i), vals[i][0] ? vals[i] : 0, -1, kinds[hs_kind(i)]};
    }
    return n;
}

/* ---- the frame rate (HOM-NFR-001): home.log, so a slow screen shows in numbers ---- */

typedef struct {
    unsigned int t0, frames, cpu_us, ge_us, warm_us, worst_ms, last, late, made;
} stats;

static void stats_frame(stats *st, unsigned int now, unsigned int cpu_us, unsigned int warm_us,
                        unsigned int ge_us)
{
    unsigned int made, kept, bytes;

    if (st->frames) {
        if (now - st->last > st->worst_ms)
            st->worst_ms = now - st->last;
        if (now - st->last > 20) /* more than one vblank: a frame missed */
            st->late++;
    }
    hm_gu_stats(&made, &kept, &bytes);
    st->last = now;
    st->frames++;
    st->cpu_us += cpu_us;
    st->warm_us += warm_us;
    st->ge_us += ge_us;
    st->made += made;
}

static void stats_write(stats *st, unsigned int now, const char *why)
{
    char line[240];
    unsigned int ms = now - st->t0, n = st->frames ? st->frames : 1;
    SceUID fd;

    if (st->frames < 2 || ms == 0)
        return;
    {
        unsigned int made, kept, bytes;

        hm_gu_stats(&made, &kept, &bytes);
        stbsp_snprintf(line, sizeof(line),
                       "home %s: %u frames in %u ms (%u fps), %u late; a frame: CPU %u us, warm "
                       "%u us, GE wait %u us; longest %u ms; stamps %u made, %u kept (%u KB)\n",
                       why, st->frames, ms, st->frames * 1000 / ms, st->late, st->cpu_us / n,
                       st->warm_us / n, st->ge_us / n, st->worst_ms, st->made, kept, bytes >> 10);
    }
    fd = sceIoOpen(HOME_LOG, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, line, strlen(line));
        sceIoClose(fd);
    }
    memset(st, 0, sizeof(*st));
    st->t0 = st->last = hm_hw_now_ms();
}

/* ---- USB (HOM-REQ-011): the card belongs to the computer meanwhile ---- */

static void usb_texts(hm_view *v, int state)
{
    v->sheet_title = pw_tr(PW_S_HM_USB);
    v->sheet_text = pw_tr(state == HM_USB_CONNECTED  ? PW_S_HM_USB_ON
                          : state == HM_USB_NO_CABLE ? PW_S_HM_USB_CABLE
                          : state == HM_USB_FAILED   ? PW_S_HM_USB_FAIL
                                                     : PW_S_HM_USB_WAIT);
    v->sheet_busy = state == HM_USB_CONNECTING;
    v->sheet_hint = pw_tr(PW_S_HM_USB_END);
}

/* ---- the HOME panel (HOM-REQ-010) and a game's options (HOM-REQ-014, -015) ---- */

enum { PANEL_CLOSE, PANEL_USB, PANEL_XMB, PANEL_SLEEP, PANEL_OFF, PANEL_ITEMS };
enum { SHEET_NONE, SHEET_HOME, SHEET_GAME, SHEET_INFO };

/* A picture's time in the slideshow by Settings -> Slideshow Speed: normal, fast, slow
   (INFERRED from the XMB's Photo Settings; its exact times are not documented). */
static const unsigned int k_slide_ms[3] = {5000, 3000, 10000};

static void panel_home(hm_view *v)
{
    memset(v->panel_values, 0, sizeof(v->panel_values));
    memset(v->panel_icons, 0, sizeof(v->panel_icons));
    v->panel_title = "Pocketwatch";
    v->panel_items[PANEL_CLOSE] = pw_tr(PW_S_HM_CLOSE);
    v->panel_items[PANEL_USB] = pw_tr(PW_S_HM_USB);
    v->panel_items[PANEL_XMB] = pw_tr(PW_S_HM_EXIT);
    v->panel_items[PANEL_SLEEP] = pw_tr(PW_S_HM_SLEEP);
    v->panel_items[PANEL_OFF] = pw_tr(PW_S_HM_POWER_OFF);
    v->panel_count = PANEL_ITEMS;
    v->panel_hint = 0;
}

static hm_state g_states[HM_STATES_MAX];
static int g_nstates;
static char g_state_names[HM_STATES_MAX][40];

/* A game's options: its save states with their pictures (newest first), then Saved Data. */
static void panel_game(hm_view *v, const hm_game *g)
{
    hm_states_free(g_states, g_nstates);
    g_nstates = g->kind <= HM_KIND_PSN && g->id[0] ? hm_states_list(g->id, g_states) : 0;
    memset(v->panel_values, 0, sizeof(v->panel_values));
    memset(v->panel_icons, 0, sizeof(v->panel_icons));
    v->panel_title = g->title;
    for (int i = 0; i < g_nstates; i++) {
        stbsp_snprintf(g_state_names[i], sizeof(g_state_names[i]), pw_tr(PW_S_SLOT_TITLE),
                       g_states[i].slot);
        v->panel_items[i] = g_state_names[i];
        v->panel_values[i] = g_states[i].when;
        v->panel_icons[i] = g_states[i].icon;
    }
    v->panel_count = g_nstates;
    if (g->kind <= HM_KIND_PSN && g->id[0]) /* PSP games keep their saves under their disc ID */
        v->panel_items[v->panel_count++] = pw_tr(PW_S_HM_SAVED_DATA);
    v->panel_items[v->panel_count++] = pw_tr(PW_S_HM_INFORMATION);
    v->panel_hint = 0;
}

/* A game's Information: what the XMB's Triangle -> Information shows, and its file. */
static char g_info_vals[2][32];
static void panel_info(hm_view *v, const hm_game *g)
{
    const char *slash = strrchr(g->path, '/');
    int n = 0;

    memset(v->panel_values, 0, sizeof(v->panel_values));
    memset(v->panel_icons, 0, sizeof(v->panel_icons));
    v->panel_title = g->title;
    if (g->id[0]) {
        v->panel_items[n] = pw_tr(PW_S_HM_DISC_ID);
        v->panel_values[n++] = g->id;
    }
    if (g->version[0]) {
        v->panel_items[n] = pw_tr(PW_S_HM_VERSION);
        v->panel_values[n++] = g->version;
    }
    if (g->sysver[0]) {
        v->panel_items[n] = pw_tr(PW_S_HM_FIRMWARE);
        v->panel_values[n++] = g->sysver;
    }
    if (g->size_kb >= 1024)
        stbsp_snprintf(g_info_vals[0], sizeof(g_info_vals[0]), "%u MB", (g->size_kb + 512) / 1024);
    else
        stbsp_snprintf(g_info_vals[0], sizeof(g_info_vals[0]), "%u KB", g->size_kb);
    v->panel_items[n] = pw_tr(PW_S_HM_SIZE);
    v->panel_values[n++] = g_info_vals[0];
    if (g->play_s >= 60) {
        duration(g_info_vals[1], sizeof(g_info_vals[1]), g->play_s);
        v->panel_items[n] = pw_tr(PW_S_PLAYTIME);
        v->panel_values[n++] = g_info_vals[1];
    }
    v->panel_items[n] = pw_tr(PW_S_HM_FILE);
    v->panel_values[n++] = slash ? slash + 1 : g->path;
    v->panel_count = n;
    v->panel_hint = 0;
}

/* Home's background without a game's art: the wallpaper (hsettings.c), or the slate gradient. */
static void make_default_background(void)
{
    if (!g_gradient)
        return;
    if (hs_wallpaper_load(g_gradient) < 0)
        hm_make_gradient(g_gradient);
    hm_gu_written(g_gradient, 512 * HM_H * 4);
    hm_gu_upload_bg(2, g_gradient); /* slots 0-1: the loader's backgrounds */
}

/* Every text the screen holds, again after the language changed. */
static void texts(hm_view *v, int sheet)
{
    v->tab_names[HM_TAB_GAMES] = pw_tr(PW_S_HM_GAMES);
    v->tab_names[HM_TAB_MUSIC] = pw_tr(PW_S_HM_MUSIC);
    v->tab_names[HM_TAB_PHOTOS] = pw_tr(PW_S_HM_PHOTOS);
    v->tab_names[HM_TAB_PLAYTIME] = pw_tr(PW_S_PLAYTIME);
    v->tab_names[HM_TAB_SETTINGS] = pw_tr(PW_S_SETTINGS);
    v->tab_names[HM_TAB_SYSTEM] = pw_tr(PW_S_HM_SYSTEM);
    v->play_label = pw_tr(PW_S_HM_PLAY);
    v->empty = pw_tr(PW_S_HM_NO_GAMES);
    v->hint_tabs = pw_tr(PW_S_HM_SECTION);
    if (sheet == SHEET_HOME)
        panel_home(v);
}

/* A firmware dialog over Home: Home's screen as it is, each vblank. */
static void dialog_frame(void *ctx)
{
    hm_draw(hm_gu_begin(), &g_ui, (const hm_view *)ctx);
    hm_gu_finish();
}

/* ---- the loop ---- */

int main(void)
{
    static hm_view v;
    pw_keys keys = {0, 0, 0};
    stats st;
    unsigned int t_prev, t_open, t_fade = 0, t_rows = 0, t_status = 0, t_usb = 0;
    const unsigned int *shown = 0; /* the background drawn last */
    int launching = -1, launch_slot = 0, exit_row = -1, usb_row = -1, row_sel = 0, up = 0,
        sheet = SHEET_NONE, leave = 0, usb = 0, usb_state = HM_USB_OFF;
    int g_sel = 0, g_selpos = 0, ph_sel = 0, ph_selpos = 0,
        viewer = 0; /* the two rows of pictures */
    int slideshow = 0;
    unsigned int t_caption = 0, t_slide = 0;
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
    pw_config_load(PW_DIR "config.txt");
    hm_hw_model(g_model, sizeof(g_model));
    hm_hw_firmware(g_firmware, sizeof(g_firmware));
    hm_hw_cfw(g_cfw, sizeof(g_cfw));
    hm_hw_mac(g_mac, sizeof(g_mac));
    g_gradient = memalign(64, 512 * HM_H * 4);
    make_default_background();
    hm_library_sort(pw_config_get()->sort); /* (no games yet: only the order to use) */
    hm_library_scan();
    build_tiles();
    hm_library_start();
    hm_music_scan();

    v.clock = clock;
    v.bg = g_gradient;
    v.bg_mix = 256;
    v.other_rows[HM_TAB_MUSIC] = g_mu_rows;
    v.other_rows[HM_TAB_PLAYTIME] = g_pt_rows;
    v.other_rows[HM_TAB_SETTINGS] = g_set_rows;
    v.other_rows[HM_TAB_SYSTEM] = g_sys_rows;
    texts(&v, sheet);
    shown = g_gradient;
    t_open = t_prev = hm_hw_now_ms();
    memset(&st, 0, sizeof(st));
    st.t0 = st.last = t_open;

    while (!hm_hw_exit_requested() && !leave) {
        unsigned int now = hm_hw_now_ms(), t_cpu = sceKernelGetSystemTimeLow(), t_warm,
                     ev = pw_keys_update(&keys, hm_hw_buttons(), REPEAT, now * 1000);
        const unsigned int *want;
        int dt = (int)(now - t_prev), start_usb = 0, nickname = 0, saved_data = 0;
        hm_game *gsel = hm_library_count() ? hm_library_game(g_sel) : 0;
        hm_gfx *g;

        t_prev = now;
        /* input: USB, then the open panel, over everything */
        if (usb) {
            if (ev & (back | PSP_CTRL_HOME)) { /* the card is ours again: read it afresh */
                hm_usb_stop();
                usb = 0;
                hm_library_rescan();
                build_tiles();
                g_sel = hm_library_count()
                            ? (g_sel < hm_library_count() ? g_sel : hm_library_count() - 1)
                            : 0;
                g_selpos = g_sel * 256;
                ph_sel = ph_selpos = 0;
                hm_library_count_free();
                hm_library_pause(0);
                t_rows = 0;
                shown = 0; /* the background again for what is now chosen */
            }
        } else if (launching < 0 && (ev & PSP_CTRL_HOME)) {
            sheet = sheet == SHEET_HOME ? SHEET_NONE : SHEET_HOME;
            if (sheet == SHEET_HOME)
                panel_home(&v);
            v.panel_sel = 0;
        } else if (launching < 0 && sheet != SHEET_NONE) {
            if (ev & PSP_CTRL_UP)
                v.panel_sel = v.panel_sel > 0 ? v.panel_sel - 1 : 0;
            else if (ev & PSP_CTRL_DOWN)
                v.panel_sel = v.panel_sel < v.panel_count - 1 ? v.panel_sel + 1 : v.panel_sel;
            else if ((ev & back) && sheet == SHEET_INFO) { /* back to the game's options */
                panel_game(&v, gsel);
                sheet = SHEET_GAME;
                v.panel_sel = v.panel_count - 1;
            } else if (ev & (back | (sheet != SHEET_HOME ? PSP_CTRL_TRIANGLE : 0)))
                sheet = SHEET_NONE;
            else if ((ev & enter) && sheet == SHEET_HOME) {
                sheet = SHEET_NONE;
                if (v.panel_sel == PANEL_USB)
                    start_usb = 1;
                else if (v.panel_sel == PANEL_XMB)
                    leave = 1;
                else if (v.panel_sel == PANEL_SLEEP)
                    scePowerRequestSuspend();
                else if (v.panel_sel == PANEL_OFF)
                    scePowerRequestStandby();
            } else if ((ev & enter) && sheet == SHEET_GAME && gsel) {
                sheet = SHEET_NONE;
                if (v.panel_sel < g_nstates) { /* continue from that state */
                    launching = g_sel;
                    launch_slot = g_states[v.panel_sel].slot;
                    t_fade = now;
                } else if (v.panel_sel == v.panel_count - 1) { /* the last: Information */
                    sheet = SHEET_INFO;
                    panel_info(&v, gsel);
                    v.panel_sel = 0;
                } else {
                    saved_data = 1;
                }
            }
            v.panel_hint =
                sheet == SHEET_GAME && v.panel_sel < g_nstates ? pw_tr(PW_S_HM_CONTINUE) : 0;
        } else if (launching < 0) {
            if (!viewer && (ev & PSP_CTRL_LTRIGGER))
                v.tab = (v.tab + HM_TABS - 1) % HM_TABS;
            else if (!viewer && (ev & PSP_CTRL_RTRIGGER))
                v.tab = (v.tab + 1) % HM_TABS;
            if (ev & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) {
                row_sel = v.tab == HM_TAB_SETTINGS ? hs_next(-1, 1) : 0;
                v.row_pos = row_sel * 256;
            }
            if (viewer) { /* a photo whole: left / right through them, any other button back */
                int n = hm_library_photo_count();

                if ((ev & PSP_CTRL_LEFT) && ph_sel > 0)
                    ph_sel--;
                else if ((ev & PSP_CTRL_RIGHT) && ph_sel < n - 1)
                    ph_sel++;
                else if (ev & PSP_CTRL_TRIANGLE) /* a slideshow from here, or not */
                    slideshow = !slideshow;
                else if ((ev & PSP_CTRL_SQUARE) && g_gradient) { /* this one behind Home */
                    const char *path = hm_library_photo(ph_sel)->path;

                    if (hm_library_wallpaper(path, g_gradient) == 0)
                        hs_wallpaper_set(path, g_gradient);
                    make_default_background(); /* (the old one when that failed) */
                } else if (ev & (enter | back))
                    viewer = slideshow = 0;
                if (ev & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT | PSP_CTRL_TRIANGLE | PSP_CTRL_SQUARE)) {
                    t_caption = now;
                    t_slide = now;
                }
            } else if (v.tab == HM_TAB_PHOTOS && hm_library_photo_count() > 0) {
                if (ev & PSP_CTRL_LEFT)
                    ph_sel = ph_sel > 0 ? ph_sel - 1 : 0;
                else if (ev & PSP_CTRL_RIGHT)
                    ph_sel = ph_sel < hm_library_photo_count() - 1 ? ph_sel + 1 : ph_sel;
                else if (ev & enter) {
                    viewer = 1;
                    t_caption = now;
                }
            } else if (v.tab == HM_TAB_GAMES && hm_library_count() > 0) {
                if (ev & PSP_CTRL_LEFT)
                    g_sel = g_sel > 0 ? g_sel - 1 : 0;
                else if (ev & PSP_CTRL_RIGHT)
                    g_sel = g_sel < hm_library_count() - 1 ? g_sel + 1 : g_sel;
                else if (ev & enter) {
                    launching = g_sel;
                    launch_slot = 0;
                    t_fade = now;
                } else if ((ev & PSP_CTRL_TRIANGLE) && gsel) {
                    panel_game(&v, gsel);
                    sheet = SHEET_GAME;
                    v.panel_sel = 0;
                    v.panel_pos = 0;
                    v.panel_hint = g_nstates ? pw_tr(PW_S_HM_CONTINUE) : 0;
                }
            } else if (v.tab == HM_TAB_MUSIC) {
                if (ev & PSP_CTRL_UP)
                    row_sel = row_sel > 0 ? row_sel - 1 : 0;
                else if (ev & PSP_CTRL_DOWN)
                    row_sel = row_sel < v.nrows - 1 ? row_sel + 1 : row_sel;
                else if (ev & PSP_CTRL_LEFT)
                    hm_music_skip(-1);
                else if (ev & PSP_CTRL_RIGHT)
                    hm_music_skip(1);
                else if (ev & PSP_CTRL_SQUARE)
                    hm_music_stop();
                else if (ev & enter) {
                    if (row_sel == 0 || row_sel - 1 == hm_music_current())
                        hm_music_toggle();
                    else
                        hm_music_play(row_sel - 1);
                }
                t_rows = 0;
            } else if (v.tab == HM_TAB_SETTINGS) {
                int r;

                if (ev & PSP_CTRL_UP)
                    row_sel = hs_next(row_sel, -1);
                else if (ev & PSP_CTRL_DOWN)
                    row_sel = hs_next(row_sel, 1);
                else if (ev & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) {
                    int was = pw_config_get()->sort;

                    r = hs_step(row_sel, ev & PSP_CTRL_LEFT ? -1 : 1);
                    if (pw_config_get()->sort != was) { /* the games in their new order */
                        hm_library_sort(pw_config_get()->sort);
                        build_tiles();
                        g_sel = 0;
                        g_selpos = 0;
                    }
                    if (r == HS_WALLPAPER_CHANGED)
                        make_default_background();
                    if (r == HS_TEXTS_CHANGED) { /* a language: every text again */
                        hm_texts_load();
                        texts(&v, sheet);
                        build_tiles();
                    }
                    t_rows = 0;
                } else if ((ev & enter) && hs_action(row_sel) == HS_ACT_NICKNAME) {
                    nickname = 1;
                }
            } else {
                if (ev & PSP_CTRL_UP)
                    row_sel = row_sel > 0 ? row_sel - 1 : 0;
                else if (ev & PSP_CTRL_DOWN)
                    row_sel = row_sel < v.nrows - 1 ? row_sel + 1 : row_sel;
                else if ((ev & enter) && v.tab == HM_TAB_SYSTEM && row_sel == exit_row)
                    leave = 1;
                else if ((ev & enter) && v.tab == HM_TAB_SYSTEM && row_sel == usb_row)
                    start_usb = 1;
            }
        }
        if (start_usb) { /* the loader and the music leave the card first */
            hm_music_stop();
            hm_library_pause(1);
            usb = 1;
            usb_state = hm_usb_start(now);
            t_usb = now;
        }
        if (viewer && slideshow && now - t_slide >= k_slide_ms[pw_config_get()->slide % 3]) {
            /* the next, round again */
            t_slide = now;
            ph_sel = hm_library_photo_count() > 0 ? (ph_sel + 1) % hm_library_photo_count() : 0;
        }
        if (usb && now - t_usb >= 250) {
            t_usb = now;
            usb_state = hm_usb_poll(now);
        }
        if (usb)
            usb_texts(&v, usb_state);
        /* the firmware's own dialogs, over Home's screen */
        if (nickname) {
            char was[64], typed[64];

            hs_nickname(was, sizeof(was));
            if (hm_dialog_osk(pw_tr(PW_S_HS_NICKNAME), was, typed, sizeof(typed), g_ui.cross_enter,
                              hm_hw_language(), dialog_frame, &v) == 1)
                hs_set_nickname(typed);
            t_rows = 0;
            t_prev = hm_hw_now_ms();
            continue;
        }
        if (saved_data && gsel) {
            hm_dialog_savedata(gsel->id, g_ui.cross_enter, hm_hw_language(), dialog_frame, &v);
            t_prev = hm_hw_now_ms();
            continue;
        }
        /* what changes by the second: the clock, the battery, the cards' rows */
        if (t_status == 0 || now - t_status >= 1000) {
            t_status = now;
            hm_hw_clock(clock, sizeof(clock));
            v.battery = hm_hw_battery(&v.charging);
        }
        if (t_rows == 0 || now - t_rows >= 1000) { /* all, so the others can be prepared */
            t_rows = now;
            v.other_nrows[HM_TAB_MUSIC] = music_rows();
            v.other_nrows[HM_TAB_PLAYTIME] = playtime_rows(g_pt_rows, g_pt_vals);
            v.other_nrows[HM_TAB_SETTINGS] = settings_rows(g_set_rows, g_set_vals);
            v.other_nrows[HM_TAB_SYSTEM] = system_rows(g_sys_rows, g_sys_vals, &usb_row, &exit_row);
        }
        v.rows = v.other_rows[v.tab];
        v.nrows = v.other_nrows[v.tab];
        if (row_sel >= v.nrows)
            row_sel = v.nrows > 0 ? v.nrows - 1 : 0;
        v.row_sel = row_sel;
        /* motion, by time */
        g_selpos = pw_ease(g_selpos, g_sel * 256, dt, PW_ANIM_TAU_MS, 6);
        ph_selpos = pw_ease(ph_selpos, ph_sel * 256, dt, PW_ANIM_TAU_MS, 6);
        v.tab_pos = pw_ease(v.tab_pos, v.tab * 256, dt, PW_ANIM_TAU_MS, 6);
        v.row_pos = pw_ease(v.row_pos, row_sel * 256, dt, PW_ANIM_TAU_MS, 6);
        v.panel = pw_ease(v.panel, sheet != SHEET_NONE ? 256 : 0, dt, PW_ANIM_TAU_MS, 6);
        v.panel_pos = pw_ease(v.panel_pos, v.panel_sel * 256, dt, PW_ANIM_TAU_MS, 6);
        v.sheet = pw_ease(v.sheet, usb ? 256 : 0, dt, PW_ANIM_TAU_MS, 6);
        hm_library_focus(g_sel);
        hm_library_mode(v.tab == HM_TAB_PHOTOS ? (viewer ? 2 : 1) : 0, ph_sel);
        for (int i = 0; i < hm_library_count(); i++)
            g_tiles[i].icon = hm_library_game(i)->icon;
        for (int i = 0; i < hm_library_photo_count(); i++) {
            hm_photo *ph = hm_library_photo(i);

            g_ph_tiles[i] = (hm_tile){ph->name, ph->meta, ph->thumb};
        }
        /* the row on screen: the games or the photos */
        if (v.tab == HM_TAB_PHOTOS) {
            v.tiles = g_ph_tiles;
            v.count = hm_library_photo_count();
            v.sel = ph_sel;
            v.selpos = ph_selpos;
            v.empty = pw_tr(PW_S_HM_PH_NONE);
            v.play_label = pw_tr(PW_S_HM_VIEW);
        } else {
            v.tiles = g_tiles;
            v.count = hm_library_count();
            v.sel = g_sel;
            v.selpos = g_selpos;
            v.empty = pw_tr(PW_S_HM_NO_GAMES);
            v.play_label = pw_tr(PW_S_HM_PLAY);
        }
        v.viewer = viewer && v.tab == HM_TAB_PHOTOS ? 256 : 0;
        v.viewer_caption = v.viewer ? g_ph_tiles[ph_sel].title : 0;
        v.viewer_slideshow = pw_tr(PW_S_HM_SLIDESHOW);
        v.viewer_wallpaper = pw_tr(PW_S_HM_WP_SET);
        /* the photo's name and the buttons: 2 s after a change, then they fade out */
        v.caption = now - t_caption < 2000 ? 256 : 256 - pw_ramp(now, t_caption + 2000, 400);
        /* the chosen game's (or photo's) art, faded in once the loader has it */
        if (v.tab == HM_TAB_PHOTOS)
            want = v.count ? hm_library_photo_background(ph_sel, viewer) : 0;
        else if (v.tab == HM_TAB_GAMES)
            want = hm_library_count() ? hm_library_background(g_sel) : 0;
        else /* the other sections: the wallpaper, as in the XMB */
            want = g_gradient;
        if (!want)
            want = viewer ? shown : g_gradient;
        if (want != shown && want) {
            v.bg_prev = shown;
            v.bg = want;
            v.bg_mix = shown ? 0 : 256;
            shown = want;
        }
        v.bg_mix = v.bg_mix + dt * 256 / MIX_MS > 256 ? 256 : v.bg_mix + dt * 256 / MIX_MS;
        v.hint_enter =
            (v.tab == HM_TAB_GAMES && v.count) || (v.tab == HM_TAB_MUSIC && hm_music_count())
                ? v.play_label
            : v.tab == HM_TAB_PHOTOS && v.count ? pw_tr(PW_S_HM_VIEW)
                                                : 0;
        v.hint_options = v.tab == HM_TAB_GAMES && gsel ? pw_tr(PW_S_HM_OPTIONS) : 0;
        v.t_ms = now;
        v.appear =
            launching >= 0 ? 256 - pw_ramp(now, t_fade, FADE_MS) : pw_ramp(now, t_open, FADE_MS);

        /* the frame: the list, then - while the GE draws it - what the next frames may need */
        g = hm_gu_begin();
        hm_draw(g, &g_ui, &v);
        hm_gu_finish();
        t_warm = sceKernelGetSystemTimeLow();
        t_cpu = t_warm - t_cpu;
        hm_prefetch(g, &g_ui, &v);
        t_warm = sceKernelGetSystemTimeLow() - t_warm;
        stats_frame(&st, now, t_cpu, t_warm, hm_gu_end());
        if (!up) { /* the first screen is up: tell the XMB plugin's guard */
            hm_hw_remove(PENDING);
            up = 1;
        }
        if (!usb && now - st.t0 >= STATS_MS)
            stats_write(&st, now, "running");

        if (launching >= 0 && now - t_fade >= FADE_MS) {
            hm_game *gm = hm_library_game(launching);

            stats_write(&st, now, "before a game");
            hm_music_stop();
            if (launch_slot > 0 && hm_states_continue(gm->id, launch_slot) < 0)
                launch_slot = 0; /* continue.txt not written: the game starts plainly */
            if (gm->kind == HM_KIND_ISO)
                hm_launch(gm->path, hm_library_has(gm->path, "PSP_GAME/SYSDIR/EBOOT.OLD"));
            else if (gm->kind == HM_KIND_PSN)
                hm_launch_psn(gm->path);
            else
                hm_launch_eboot(gm->path, gm->kind == HM_KIND_PS1);
            if (launch_slot > 0) /* it returned: the start failed; no state for a later start */
                hm_hw_remove(CONTINUE);
            launching = -1;
            t_open = now;
        }
    }
    if (usb)
        hm_usb_stop();
    stats_write(&st, hm_hw_now_ms(), "leaving");
    hm_hw_touch(XMB_ONCE); /* leaving Home means the XMB, even when Home is the start screen */
    sceKernelExitGame();
    return 0;
}
