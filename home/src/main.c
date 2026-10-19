// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Pocketwatch Home (docs/subsystems/home.md, ADR-025): a home screen of our own - the games on
 * the card with their art, play time, save states and saved data, the music, the photos, the
 * settings and the system - drawn in the in-game menu's three styles (ADR-033), every section a
 * stack of pages (ADR-035, pages.c). A homebrew application: started from the XMB (Game ->
 * Memory Stick), or - XMB -> Settings -> Pocketwatch -> Start screen - by ARK-4 instead of the
 * XMB (ADR-026). Games start the way ARK-4's own menu starts them; Exit to the XMB and HOME ->
 * Quit leave a marker so that the next start shows the XMB, not Home again.
 */
#include <malloc.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include <stdlib.h>
#include <string.h>

#include "core/config.h"
#include "dialogs.h"
#include "gfx/image.h"
#include "gfx/pgf.h"
#include "gfx/text.h"
#include "gfx_gu.h"
#include "hcfg.h"
#include "hm_icons.h"
#include "hm_image.h"
#include "home_view.h"
#include "hsettings.h"
#include "hw.h"
#include "launch.h"
#include "library.h"
#include "music.h"
#include "net.h"
#include "pages.h"
#include "photo_edit.h"
#include "sound.h"
#include "stb_sprintf.h"
#include "texts.h"
#include "ui/anim.h"
#include "ui/i18n.h"
#include "ui/icons.h"
#include "ui/input.h"
#include "usb.h"
#include "xmbart.h"

PSP_MODULE_INFO("PocketwatchHome", PSP_MODULE_USER, 0, 1);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
/* The heap: all the user memory free at the first malloc but 4 MB, which stay for the firmware
   (dialogs, the MP3 decoder's and the network's modules, their threads). A negative heap size
   alone keeps only libcglue's default 512 KB (glue.c _sbrk_r: MaxFreeMemSize - threshold):
   PPSSPP showed 200 KB left and sceNetInit failing 0x80020190 with PSP_HEAP_SIZE_KB(-4096). */
PSP_HEAP_SIZE_KB(-1);
PSP_HEAP_THRESHOLD_SIZE_KB(4096);

#define FONT_PATH "flash0:/font/ltn0.pgf"
#define RES_PATH  "flash0:/vsh/resource/" /* the XMB's pictures (Classic, ADR-033) */
#define PW_DIR    "ms0:/seplugins/pocketwatch/"
#define CONTINUE  PW_DIR "continue.txt" /* read by the game plugin (M7 V1) */
#define XMB_ONCE  PW_DIR "xmb-once"     /* read by the XMB plugin (vsh/src/xboot.c) */
#define PENDING   PW_DIR "boot-pending" /* the XMB plugin's crash guard: Home did come up */
#define HOME_LOG  PW_DIR "home.log"     /* the frame rate, every STATS_MS */
#define HOME_CFG  PW_DIR "home.txt"     /* Home's own settings (hcfg.h) */
#define BROWSER   PW_DIR "browser.txt"  /* the browser's first page (a URL), if not the default */
/* A search made for old browsers (plain HTTP; the PSP's browser has no modern TLS). PROPOSED. */
#define BROWSER_HOME "http://frogfind.com/"
#define STATS_MS     30000
#define FADE_MS      250
#define MIX_MS       300 /* a new background fades in */
#define TOAST_MS     1800
#define HOLD_MS      450 /* left / right held this long: seeking, not the next track */
#define SEEK_EVERY   220
#define SEEK_STEP    5    /* s */
#define EDIT_SIDE    1600 /* an edited copy's longest side at most (memory) */
#define REPEAT       (PSP_CTRL_LEFT | PSP_CTRL_RIGHT | PSP_CTRL_UP | PSP_CTRL_DOWN)

static hm_ui g_ui;
static pw_font g_fonts[3];
static char g_model[24], g_firmware[16], g_cfw[32], g_mac[24]; /* read once */
static unsigned int *g_gradient;                               /* the default background */
static hm_art g_art;
static pw_image g_sky;
static unsigned char *g_sky_file; /* the month skies' BMP (kept: the Sky Colour setting) */
static unsigned int g_sky_len;
static int g_sky_shown;                   /* the default background is the XMB's sky */
static int g_aurora_shown;                /* ... or the aurora's dark sky */
static char g_pop_text[HM_POPUP_MAX][32]; /* a choice's options (Classic) */
static hm_view g_v;
static hm_page_id g_stack[HM_DEPTH];
static hm_page_env g_env;
static unsigned int g_enter, g_back; /* the system's Enter and Back buttons */

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

/* ---- the frame rate (HOM-NFR-001): home.log, so a slow screen shows in numbers ---- */

typedef struct {
    unsigned int t0, frames, cpu_us, ge_us, warm_us, worst_ms, last, late, made, rows_us;
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

static void log_line(const char *line)
{
    SceUID fd = sceIoOpen(HOME_LOG, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);

    if (fd >= 0) {
        sceIoWrite(fd, line, strlen(line));
        sceIoClose(fd);
    }
}

/* What the battery's calls cost: once, so a slow one shows by name. */
static int g_costs_written;

static void stats_write(stats *st, unsigned int now, const char *why)
{
    char line[300];
    unsigned int ms = now - st->t0, n = st->frames ? st->frames : 1;

    if (st->frames < 2 || ms == 0)
        return;
    if (!g_costs_written) {
        char cost[200];

        if (hm_hw_battery_cost(cost, sizeof(cost))) {
            stbsp_snprintf(line, sizeof(line), "home reads: battery: %s\n", cost);
            log_line(line);
            g_costs_written = 1;
        }
    }
    {
        unsigned int made, kept, bytes;
        struct mallinfo mi = mallinfo(); /* Home's heap; and the memory left outside it */

        hm_gu_stats(&made, &kept, &bytes);
        stbsp_snprintf(line, sizeof(line),
                       "home %s: %u frames in %u ms (%u fps), %u late; a frame: CPU %u us, warm "
                       "%u us, GE wait %u us; longest %u ms; rows %u us; stamps %u made, %u kept "
                       "(%u KB); heap %u of %u KB, outside %u KB\n",
                       why, st->frames, ms, st->frames * 1000 / ms, st->late, st->cpu_us / n,
                       st->warm_us / n, st->ge_us / n, st->worst_ms, st->rows_us, st->made, kept,
                       bytes >> 10, (unsigned int)mi.uordblks >> 10, (unsigned int)mi.arena >> 10,
                       (unsigned int)sceKernelMaxFreeMemSize() >> 10);
    }
    log_line(line);
    memset(st, 0, sizeof(*st));
    st->t0 = st->last = hm_hw_now_ms();
}

/* ---- USB (HOM-REQ-011): the card belongs to the computer meanwhile ---- */

static void usb_texts(hm_view *v, int state, int again)
{
    v->sheet_title = pw_tr(PW_S_HM_USB);
    v->sheet_text =
        pw_tr(state == HM_USB_CONNECTED  ? PW_S_HM_USB_ON
              : state == HM_USB_NO_CABLE ? PW_S_HM_USB_CABLE
              : state == HM_USB_FAILED   ? (hm_usb_busy() ? PW_S_HM_USB_BUSY : PW_S_HM_USB_FAIL)
                                         : PW_S_HM_USB_WAIT);
    v->sheet_busy = state == HM_USB_CONNECTING;
    v->sheet_hint = pw_tr(PW_S_HM_USB_END);
    v->sheet_again = again ? pw_tr(PW_S_HM_USB_AGAIN) : 0;
    v->sheet_glyph = HM_ICON_USB;
}

static void usb_log(void)
{
    char line[400];
    int n = hm_usb_report(line, (int)sizeof(line) - 1);

    line[n] = '\n';
    line[n + 1] = 0;
    log_line(line);
}

/* ---- the HOME panel (HOM-REQ-010) ---- */

enum { PANEL_CLOSE, PANEL_USB, PANEL_XMB, PANEL_SLEEP, PANEL_OFF, PANEL_ITEMS };

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

/* ---- the look: the style, the theme, the background ---- */

/* config.txt's style as Home draws it */
static int style_now(void)
{
    int s = pw_config_get()->style;

    return s >= 0 && s < HM_STYLES ? s : HM_STYLE_CLASSIC;
}

/* The sky of the Sky Colour setting: the XMB's theme colour (or the month), or a month's. */
static void choose_sky(void)
{
    static unsigned int px[64 * 64];
    int colour, index = hm_cfg_get()->sky >= 1 && hm_cfg_get()->sky <= 12
                            ? hm_cfg_get()->sky - 1
                            : hm_hw_sky_index(&colour);

    if (g_sky_file && (pw_bmp_decode(g_sky_file, g_sky_len, index, px, 64 * 64, &g_sky) == 0 ||
                       pw_bmp_decode(g_sky_file, g_sky_len, 0, px, 64 * 64, &g_sky) == 0))
        return;
    g_sky.px = 0;
}

/* Each month's colour (the swatches of Sky Colour): its picture's average. */
static void sky_swatches(void)
{
    static unsigned int px[64 * 64];
    unsigned int colours[12];
    pw_image img;

    for (int m = 0; m < 12; m++) {
        unsigned int r = 0, g = 0, b = 0, n = 0;

        colours[m] = 0;
        if (!g_sky_file || pw_bmp_decode(g_sky_file, g_sky_len, m, px, 64 * 64, &img) < 0)
            continue;
        for (int i = 0; i < img.w * img.h; i++, n++) {
            r += img.px[i] & 0xFF;
            g += img.px[i] >> 8 & 0xFF;
            b += img.px[i] >> 16 & 0xFF;
        }
        if (n)
            colours[m] = PW_RGB(r / n, g / n, b / n);
    }
    hs_set_skies(colours);
}

/* The colours and the theme of the settings into the drawing's; the aurora's lights in the
   accent's colours (the accent, and its channels turned twice). */
static void apply_look(void)
{
    static unsigned int *glow[3];
    unsigned int c;

    g_ui.accent = hm_cfg_accent(hm_cfg_get()->accent);
    g_ui.theme = hm_cfg_get()->theme;
    c = g_ui.accent;
    for (int k = 0; k < 3; k++) {
        if (!glow[k] && !(glow[k] = memalign(16, HM_GLOW * HM_GLOW * 4)))
            return;
        hm_make_glow(c, glow[k]);
        hm_gu_written(glow[k], HM_GLOW * HM_GLOW * 4);
        g_ui.glow[k] = glow[k];
        c = (c & 0xFF) << 8 | (c >> 8 & 0xFF) << 16 | (c >> 16 & 0xFF); /* R->G->B->R */
    }
}

/* Home's background without a game's art (Settings -> Background): the wallpaper, the XMB's
   sky, the accent's gradient, or a plain one - by default the style's own (Classic: the sky). */
static void make_default_background(void)
{
    int bg = hm_cfg_get()->bg, theme = hm_cfg_get()->theme;

    if (!g_gradient)
        return;
    g_sky_shown = g_aurora_shown = 0;
    if (bg == HM_BG_WALLPAPER && hs_wallpaper_load(g_gradient) == 0)
        goto done;
    if (bg == HM_BG_AUTO || bg == HM_BG_WALLPAPER)
        bg = style_now() == HM_STYLE_CLASSIC                     ? HM_BG_SKY
             : theme == HM_THEME_OLED || theme == HM_THEME_PAPER ? HM_BG_PLAIN
                                                                 : HM_BG_GRADIENT;
    if (bg == HM_BG_SKY) { /* the XMB's: its sky, the wave over it */
        choose_sky();
        hm_make_sky(g_sky.px ? &g_sky : 0, g_gradient);
        g_sky_shown = 1;
    } else if (bg == HM_BG_GRADIENT) {
        hm_make_gradient(g_ui.accent, g_gradient);
    } else if (bg == HM_BG_AURORA) { /* the night: the lights drift over it */
        hm_make_plain(g_ui.accent, theme != HM_THEME_PAPER, g_gradient);
        g_aurora_shown = 1;
    } else {
        hm_make_plain(g_ui.accent, theme != HM_THEME_PAPER, g_gradient);
    }
done:
    hm_gu_written(g_gradient, 512 * HM_H * 4);
    hm_gu_upload_bg(2, g_gradient); /* slots 0-1: the loader's backgrounds */
}

/* ---- the system's pictures (Classic, ADR-033) ---- */

/* A whole flash0 file into the heap; what each read did goes to home.log (the PSP's first run
   read 5 of 11 pictures and no sky, 2026-10-19). */
static char g_art_reads[240];

static unsigned char *read_whole(const char *path, unsigned int *len)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    int size = fd >= 0 ? (int)sceIoLseek32(fd, 0, PSP_SEEK_END) : fd, got = 0;
    unsigned char *buf = size > 0 && size <= 512 * 1024 ? malloc((size_t)size) : 0;
    const char *name = strrchr(path, '/');
    int n = (int)strlen(g_art_reads);

    if (buf) {
        sceIoLseek32(fd, 0, PSP_SEEK_SET);
        if ((got = sceIoRead(fd, buf, (unsigned int)size)) != size) {
            free(buf);
            buf = 0;
        }
    }
    if (fd >= 0)
        sceIoClose(fd);
    stbsp_snprintf(g_art_reads + n, (int)sizeof(g_art_reads) - n, " %s %X%s",
                   name ? name + 1 : path, (unsigned int)size,
                   buf       ? ""
                   : got < 0 ? " read failed"
                             : " not taken");
    *len = buf ? (unsigned int)size : 0;
    return buf;
}

static void load_art(void)
{
    unsigned int t0 = sceKernelGetSystemTimeLow(), mask = 0;
    int colour, sky = hm_hw_sky_index(&colour), found, items = 0;
    char line[400];
    const pw_image *all[HM_TABS + 5];

    found = hm_art_load(&g_art, read_whole, RES_PATH "topmenu_icon.rco",
                        RES_PATH "system_plugin_fg.rco");
    g_sky_file = read_whole(RES_PATH "01-12_03g.bmp", &g_sky_len);
    if (!g_sky_file)
        g_sky_file = read_whole(RES_PATH "01-12.bmp", &g_sky_len);
    choose_sky();
    sky_swatches();
    for (int i = 0; i < HM_TABS; i++)
        all[i] = &g_art.cat[i];
    for (int i = 0; i < 4; i++)
        all[HM_TABS + i] = &g_art.btn[i];
    all[HM_TABS + 4] = &g_art.battery;
    for (unsigned int i = 0; i < sizeof(all) / sizeof(all[0]); i++) { /* the GE reads them */
        if (all[i]->px)
            hm_gu_written(all[i]->px, (unsigned int)(((all[i]->w + 15) & ~15) * all[i]->h * 4));
        mask |= all[i]->px ? 1u << i : 0; /* which ones (bit i) */
    }
    for (int i = 0; i < HM_ART_ITEMS; i++)
        if (g_art.item[i].px) {
            hm_gu_written(g_art.item[i].px,
                          (unsigned int)(((g_art.item[i].w + 15) & ~15) * g_art.item[i].h * 4));
            items++;
        }
    g_ui.art = &g_art;
    stbsp_snprintf(line, sizeof(line),
                   "home art: %d system picture(s) (%X, %d items), sky %d (theme colour %d)%s, "
                   "%u ms;%s\n",
                   found, mask, items, sky, colour, g_sky.px ? "" : " not found",
                   (sceKernelGetSystemTimeLow() - t0) / 1000, g_art_reads);
    log_line(line);
}

/* ---- firmware dialogs and the browser ---- */

/* Behind the browser: black (it covers the screen; Home's frame would make stamps again). */
static void black_frame(void *ctx)
{
    hm_gfx *g = hm_gu_begin();

    (void)ctx;
    g->fill(g, 0, 0, HM_W, HM_H, 0xFF000000u, 256);
    hm_gu_finish();
}

/* The browser (HOM-REQ-017): music, loader and stamps out of the way for its memory. */
static void run_browser(void)
{
    char url[200], log[200], line[260];
    int n = hm_hw_read_file(BROWSER, url, sizeof(url) - 1), r;

    while (n > 0 && (url[n - 1] == '\n' || url[n - 1] == '\r' || url[n - 1] == ' '))
        n--;
    if (n <= 0)
        n = stbsp_snprintf(url, sizeof(url), "%s", BROWSER_HOME);
    url[n] = 0;
    hm_music_stop();
    hm_library_pause(1);
    hm_gu_drop_stamps();
    r = hm_net_browser(url, g_ui.cross_enter, hm_hw_language(), black_frame, 0, log, sizeof(log));
    hm_library_pause(0);
    g_env.web_failed = r < 0;
    stbsp_snprintf(line, sizeof(line), "home browser %s: 0x%08X,%s\n", url, (unsigned int)r, log);
    log_line(line);
}

static void dialog_frame(void *ctx)
{
    hm_draw(hm_gu_begin(), &g_ui, (const hm_view *)ctx);
    hm_gu_finish();
}

/* One frame now (a message before something slow). */
static void frame_now(void)
{
    hm_draw(hm_gu_begin(), &g_ui, &g_v);
    hm_gu_finish();
    hm_gu_end();
}

/* ---- the stack of pages ---- */

static void texts(hm_view *v)
{
    v->tab_names[HM_TAB_GAMES] = pw_tr(PW_S_HM_GAMES);
    v->tab_names[HM_TAB_MUSIC] = pw_tr(PW_S_HM_MUSIC);
    v->tab_names[HM_TAB_PHOTOS] = pw_tr(PW_S_HM_PHOTOS);
    v->tab_names[HM_TAB_PLAYTIME] = pw_tr(PW_S_PLAYTIME);
    v->tab_names[HM_TAB_SETTINGS] = pw_tr(PW_S_SETTINGS);
    v->tab_names[HM_TAB_SYSTEM] = pw_tr(PW_S_HM_SYSTEM);
    v->play_label = pw_tr(PW_S_HM_PLAY);
    v->hint_tabs = pw_tr(PW_S_HM_SECTION);
}

/* Every page of the stack again (their rows and tiles), and every section's first rows. */
static void build_all(void)
{
    static hm_view other;

    for (int d = 0; d <= g_v.depth; d++)
        hm_page_build(&g_v, d, g_stack[d], &g_env);
    /* the others' first pages, for hm_prefetch: built into a view of their own */
    for (int k = 0; k < HM_TABS; k++) {
        hm_page_id id = {hm_page_root(k), 0};

        if (k == g_v.tab) {
            g_v.other_rows[k] = g_v.page[0].rows;
            g_v.other_nrows[k] = g_v.page[0].nrows;
            continue;
        }
        if (id.id == P_GAMES || id.id == P_ALBUMS)
            continue;
        if (id.id == P_MUSIC) { /* (its words are the view's: built into the shown one) */
            g_v.other_rows[k] = 0;
            continue;
        }
        memset(&other.page[0], 0, sizeof(other.page[0]));
        hm_page_build(&other, 0, id, &g_env);
        g_v.other_rows[k] = other.page[0].rows;
        g_v.other_nrows[k] = other.page[0].nrows;
    }
}

static void first_row(int d)
{
    hm_page *p = &g_v.page[d];
    int i = p->nrows ? hm_row_next(p->rows, p->nrows, -1, 1) : 0;

    p->row_sel = i < 0 ? 0 : i;
    p->row_pos = p->row_sel * 256;
}

/* A page opened over the one shown. */
static void push(int id, int arg)
{
    int d = g_v.depth + 1;

    if (d >= HM_DEPTH)
        return;
    g_stack[d].id = id;
    g_stack[d].arg = arg;
    memset(&g_v.page[d], 0, sizeof(g_v.page[d]));
    hm_page_build(&g_v, d, g_stack[d], &g_env);
    first_row(d);
    g_v.depth = d;
}

static void pop(void)
{
    if (g_v.depth > 0)
        g_v.depth--; /* (the page above stays as it was while it slides out) */
    hm_page_build(&g_v, g_v.depth, g_stack[g_v.depth], &g_env);
}

static void open_tab(int tab)
{
    g_v.tab = tab;
    g_v.depth = 0;
    g_v.depth_pos = 0;
    g_stack[0].id = hm_page_root(tab);
    g_stack[0].arg = 0;
    hm_page_build(&g_v, 0, g_stack[0], &g_env);
    if (g_v.page[0].rows)
        first_row(0);
}

static int top_id(void)
{
    return g_stack[g_v.depth].id;
}

/* ---- the loop's state ---- */

enum { CONFIRM_NONE, CONFIRM_SAVE, CONFIRM_STATE, CONFIRM_PHOTO };

static struct {
    int launching, launch_slot, leave, usb, usb_state, popup, popup_row;
    int viewer, slideshow, photo; /* the photo shown whole (its index) */
    int confirm, confirm_arg;
    int hold_dir, hold_seeked; /* left / right held on a music page */
    unsigned int hold_t0, hold_last;
    unsigned int t_fade, t_open, t_caption, t_slide, t_toast, t_usb;
    int editor, ed_dirty;
    hm_edit ed;
    unsigned int *ed_src, *ed_bg; /* the picture fitted to the screen; the preview drawn */
    int ed_w, ed_h;
    int snd;
    int screen_off;      /* HOLD: the backlight off (music on) */
    unsigned int t_hold; /* since when HOLD is on */
    int cpu_mhz;         /* the clock before the screen went off */
} g_s;

static void toast(const char *text, unsigned int now)
{
    g_v.toast_text = text;
    g_s.t_toast = now;
}

/* ---- the photo editor (HOM-REQ-026) ---- */

static const int k_ed_glyph[HM_ED_TOOLS] = {HM_ICON_ROTATE,  HM_ICON_FLIP,     HM_ICON_FILTER,
                                            HM_ICON_SUN,     HM_ICON_CONTRAST, HM_ICON_DROP,
                                            HM_ICON_PALETTE, HM_ICON_VIGNETTE, HM_ICON_CHECK};

static void editor_texts(void)
{
    static const int names[HM_ED_TOOLS] = {
        PW_S_HM_ED_ROTATE, PW_S_HM_ED_MIRROR,   PW_S_HM_ED_FILTER,
        PW_S_HM_ED_LIGHT,  PW_S_HM_ED_CONTRAST, PW_S_HM_ED_SAT,
        PW_S_HM_ED_WARMTH, PW_S_HM_ED_VIGNETTE, PW_S_HM_ED_SAVE};
    static char value[24];
    int t = g_v.ed_sel, n = 0;

    for (int i = 0; i < HM_ED_TOOLS; i++)
        g_v.ed_tools[i] = pw_tr(names[i]);
    g_v.ed_glyphs = k_ed_glyph;
    g_v.ed_count = HM_ED_TOOLS;
    value[0] = 0;
    switch (t) {
    case HM_ED_ROTATE:
        stbsp_snprintf(value, sizeof(value), "%d\xC2\xB0", g_s.ed.rotate * 90);
        break;
    case HM_ED_MIRROR:
        stbsp_snprintf(value, sizeof(value), "%s", pw_tr(g_s.ed.mirror ? PW_S_ON : PW_S_OFF));
        break;
    case HM_ED_FILTER:
        stbsp_snprintf(value, sizeof(value), "%s", pw_tr(PW_S_HM_FL_NONE + g_s.ed.filter));
        break;
    case HM_ED_LIGHT:
        n = g_s.ed.light;
        break;
    case HM_ED_CONTRAST:
        n = g_s.ed.contrast;
        break;
    case HM_ED_SAT:
        n = g_s.ed.sat;
        break;
    case HM_ED_WARMTH:
        n = g_s.ed.warmth;
        break;
    case HM_ED_VIGNETTE:
        n = g_s.ed.vignette;
        break;
    default:
        break;
    }
    if (t >= HM_ED_LIGHT && t <= HM_ED_VIGNETTE)
        stbsp_snprintf(value, sizeof(value), "%s%d", n > 0 ? "+" : "", n);
    g_v.ed_value = value[0] ? value : 0;
    g_v.ed_hint = pw_tr(PW_S_HM_ED_KEYS);
}

/* The preview: the fitted picture turned, its colours, centred on black - the background. */
static void editor_preview(void)
{
    int tw, th, fw, fh;
    unsigned int *t;

    if (!g_s.ed_src || !g_s.ed_bg)
        return;
    t = hm_edit_turn(g_s.ed_src, g_s.ed_w, g_s.ed_h, g_s.ed.rotate, g_s.ed.mirror, &tw, &th);
    if (!t)
        return;
    hm_edit_colours(&g_s.ed, t, tw, th, tw);
    fw = HM_W;
    fh = th * HM_W / tw;
    if (fh > HM_H) {
        fh = HM_H;
        fw = tw * HM_H / th;
    }
    {
        pw_canvas c = {g_s.ed_bg, HM_W, HM_H, 512};

        pw_draw_fill(&c, 0, 0, HM_W, HM_H, PW_RGB(0, 0, 0));
        pw_draw_image_scaled(&c, t, tw, th, (HM_W - fw) / 2, (HM_H - fh) / 2, fw, fh, 256);
    }
    free(t);
    hm_gu_written(g_s.ed_bg, 512 * HM_H * 4);
    hm_gu_upload_bg(2, g_s.ed_bg); /* (the default background's slot, given back after) */
}

static void editor_close(void)
{
    free(g_s.ed_src);
    free(g_s.ed_bg);
    g_s.ed_src = g_s.ed_bg = 0;
    g_s.editor = 0;
    make_default_background(); /* its slot again */
}

/* Open the editor on the photo shown: its fitted copy from the viewer's background. */
static int editor_open(void)
{
    const unsigned int *whole = hm_library_photo_background(g_s.photo, 1);
    hm_photo *ph = hm_library_photo(g_s.photo);
    int fw = HM_W, fh;

    if (!whole || !ph || ph->w <= 0 || ph->h <= 0)
        return -1;
    fh = ph->h * HM_W / ph->w;
    if (fh > HM_H) {
        fh = HM_H;
        fw = ph->w * HM_H / ph->h;
    }
    if (fw < 1 || fh < 1)
        return -1;
    g_s.ed_src = malloc((size_t)fw * (size_t)fh * 4);
    g_s.ed_bg = memalign(64, 512 * HM_H * 4);
    if (!g_s.ed_src || !g_s.ed_bg) {
        editor_close();
        return -1;
    }
    for (int y = 0; y < fh; y++) /* (the viewer made it with hm_make_photo: centred) */
        memcpy(g_s.ed_src + y * fw, whole + (y + (HM_H - fh) / 2) * 512 + (HM_W - fw) / 2,
               (size_t)fw * 4);
    g_s.ed_w = fw;
    g_s.ed_h = fh;
    memset(&g_s.ed, 0, sizeof(g_s.ed));
    g_s.editor = 1;
    g_v.ed_sel = g_v.ed_pos = 0;
    editor_preview();
    return 0;
}

typedef struct {
    SceUID fd;
    int failed;
} jpeg_out;

static void jpeg_write(void *ctx, void *data, int size)
{
    jpeg_out *o = ctx;

    if (!o->failed && sceIoWrite(o->fd, data, (unsigned int)size) != size)
        o->failed = 1;
}

/* The edit applied to the picture itself (at most EDIT_SIDE on its longest side), as a new JPEG
   beside it ("<name>_edit.jpg", a number when that exists): its path, or < 0. */
static int editor_save(char *path, int cap)
{
    hm_photo *ph = hm_library_photo(g_s.photo);
    SceUID fd;
    int w = 0, h = 0, tw, th, size, n = 0, ok = 0;
    unsigned char *file;
    unsigned int *px = 0, *t;
    const char *dot;
    jpeg_out o;

    if (!ph)
        return -1;
    hm_library_pause(1); /* its memory: the loader holds no picture meanwhile */
    fd = sceIoOpen(ph->path, PSP_O_RDONLY, 0);
    size = fd >= 0 ? (int)sceIoLseek32(fd, 0, PSP_SEEK_END) : -1;
    file = size > 0 && size <= (6 << 20) ? malloc((size_t)size) : 0;
    if (file) {
        sceIoLseek32(fd, 0, PSP_SEEK_SET);
        if (sceIoRead(fd, file, (unsigned int)size) == size)
            px = hm_png_decode(file, size, &w, &h);
        free(file);
    }
    if (fd >= 0)
        sceIoClose(fd);
    if (px && (w > EDIT_SIDE || h > EDIT_SIDE)) { /* smaller first: the memory for the turn */
        int sw = w >= h ? EDIT_SIDE : w * EDIT_SIDE / h, sh = h > w ? EDIT_SIDE : h * EDIT_SIDE / w;
        unsigned int *s = malloc((size_t)sw * (size_t)sh * 4);

        if (s)
            hm_image_fit(px, w, h, s, sw, sh);
        hm_image_free(px);
        px = s;
        w = sw;
        h = sh;
        if (!px)
            goto out;
        t = hm_edit_turn(px, w, h, g_s.ed.rotate, g_s.ed.mirror, &tw, &th);
        free(px);
    } else if (px) {
        t = hm_edit_turn(px, w, h, g_s.ed.rotate, g_s.ed.mirror, &tw, &th);
        hm_image_free(px);
    } else {
        goto out;
    }
    px = 0;
    if (!t)
        goto out;
    hm_edit_colours(&g_s.ed, t, tw, th, tw);
    dot = strrchr(ph->path, '.');
    for (n = 0; n < 100; n++) {
        SceIoStat st;
        int base = dot ? (int)(dot - ph->path) : (int)strlen(ph->path);

        if (n == 0)
            stbsp_snprintf(path, cap, "%.*s_edit.jpg", base, ph->path);
        else
            stbsp_snprintf(path, cap, "%.*s_edit%d.jpg", base, ph->path, n + 1);
        if (sceIoGetstat(path, &st) < 0)
            break;
    }
    o.fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    o.failed = o.fd < 0;
    if (!o.failed)
        ok = hm_jpeg_encode(t, tw, th, 92, jpeg_write, &o) == 0 && !o.failed;
    if (o.fd >= 0)
        sceIoClose(o.fd);
    if (!ok && o.fd >= 0)
        sceIoRemove(path);
    free(t);
out:
    hm_library_pause(0);
    return ok ? 0 : -1;
}

/* ---- the keys of each page ---- */

static int classic(void)
{
    return g_v.style == HM_STYLE_CLASSIC;
}

/* A move along a list of n tiles in a grid of cols (1: a column or a row). */
static int move_grid(int sel, int n, int cols, unsigned int ev, unsigned int back_one,
                     unsigned int on_one)
{
    if (ev & back_one)
        return sel > 0 ? sel - 1 : 0;
    if (ev & on_one)
        return sel < n - 1 ? sel + 1 : sel;
    if (cols > 1 && (ev & PSP_CTRL_UP))
        return sel >= cols ? sel - cols : sel;
    if (cols > 1 && (ev & PSP_CTRL_DOWN))
        return sel + cols < n ? sel + cols : n > 0 ? n - 1 : 0;
    return sel;
}

/* The option list of a choice (Classic): the XMB's panel on the right. */
static void popup_open(int hs)
{
    int n = hs_options(hs) < HM_POPUP_MAX ? hs_options(hs) : HM_POPUP_MAX;

    for (int o = 0; o < n; o++) {
        hs_option_text(hs, o, g_pop_text[o], sizeof(g_pop_text[o]));
        g_v.popup_items[o] = g_pop_text[o];
        g_v.popup_swatch[o] = hs_swatch(hs, o);
    }
    g_v.popup_count = n;
    g_v.popup_sel = hs_current(hs) >= 0 && hs_current(hs) < n ? hs_current(hs) : 0;
    g_v.popup_pos = g_v.popup_sel * 256;
    g_s.popup = 1;
    g_s.popup_row = hs;
}

/* A setting changed (left / right, Enter, Classic's options): what depends on it. */
static void after_setting(int r, int was_sort, int was_style)
{
    if (r < 0) {
        g_s.snd = HM_SND_ERROR;
        return;
    }
    if (pw_config_get()->sort != was_sort) { /* the games in their new order */
        hm_library_sort(pw_config_get()->sort);
        hm_pages_texts();
    }
    if (r == HS_LOOK_CHANGED || style_now() != was_style) {
        apply_look();
        if (style_now() != g_v.style) { /* Settings -> Style: Home's look at once */
            g_v.style = style_now();
            hm_library_style(g_v.style == HM_STYLE_CLASSIC);
            g_s.popup = 0;
        }
        make_default_background();
    }
    if (r == HS_MUSIC_CHANGED)
        hm_music_mode(hm_cfg_get()->shuffle, hm_cfg_get()->repeat);
    if (r == HS_KEYS_CHANGED) { /* the Enter button: Enter and Back swap at once */
        g_ui.cross_enter = hm_hw_cross_enter();
        g_enter = g_ui.cross_enter ? PSP_CTRL_CROSS : PSP_CTRL_CIRCLE;
        g_back = g_ui.cross_enter ? PSP_CTRL_CIRCLE : PSP_CTRL_CROSS;
    }
    if (r == HS_TEXTS_CHANGED) { /* a language: every text again */
        hm_texts_load();
        texts(&g_v);
        hm_pages_texts();
    }
    build_all();
}

static void launch(int game, int slot, unsigned int now)
{
    g_s.launching = game;
    g_s.launch_slot = slot;
    g_s.t_fade = now;
    g_s.snd = HM_SND_START;
}

/* Enter on row r of the page shown. */
static int enter_row(int r, unsigned int now, int *nickname, int *browser, int *start_usb)
{
    hm_row_act a = hm_page_act(g_stack[g_v.depth], r);

    switch (a.act) {
    case A_OPEN:
        if (a.arg == P_GAME || a.arg == P_STATES || a.arg == P_SAVES || a.arg == P_INFO)
            push(a.arg, a.arg2);
        else if (a.arg == P_SET_CAT)
            push(a.arg, a.arg2);
        else
            push(a.arg, a.arg);
        g_s.snd = HM_SND_OPEN;
        return 1;
    case A_SETTING: {
        int was = pw_config_get()->sort, style = style_now();

        if (classic() && hs_options(a.arg) > 0) {
            popup_open(a.arg); /* the XMB's way: Enter opens the options */
            g_s.snd = HM_SND_OPEN;
        } else {
            after_setting(hs_step(a.arg, 1), was, style);
            if (g_s.snd < 0)
                g_s.snd = HM_SND_MOVE;
        }
        return 1;
    }
    case A_NICKNAME:
        *nickname = 1;
        return 1;
    case A_USB:
        *start_usb = 1;
        return 1;
    case A_BROWSER:
        *browser = 1;
        return 1;
    case A_EXIT:
        g_s.leave = 1;
        return 1;
    case A_SLEEP:
        scePowerRequestSuspend();
        return 1;
    case A_OFF:
        scePowerRequestStandby();
        return 1;
    case A_PLAY:
        launch(a.arg, 0, now);
        return 1;
    case A_TRACK:
        if (a.arg == hm_music_current())
            hm_music_toggle();
        else
            hm_music_play(a.arg);
        g_s.snd = HM_SND_OPEN;
        return 1;
    case A_DELETE:
        g_s.confirm = CONFIRM_SAVE;
        g_s.confirm_arg = a.arg;
        g_s.snd = HM_SND_OPEN;
        return 1;
    default:
        return 0;
    }
}

/* Left / right on a music page: a tap is the next or the previous track, held it seeks. */
static unsigned int music_keys(unsigned int ev, unsigned int buttons, unsigned int now)
{
    unsigned int lr = PSP_CTRL_LEFT | PSP_CTRL_RIGHT;

    if ((ev & lr) && !g_s.hold_dir) {
        g_s.hold_dir = ev & PSP_CTRL_LEFT ? -1 : 1;
        g_s.hold_t0 = now;
        g_s.hold_seeked = 0;
    }
    if (g_s.hold_dir && (buttons & lr)) {
        if (now - g_s.hold_t0 >= HOLD_MS && now - g_s.hold_last >= SEEK_EVERY) {
            hm_music_seek(g_s.hold_dir * SEEK_STEP);
            g_s.hold_seeked = 1;
            g_s.hold_last = now;
        }
    } else if (g_s.hold_dir) { /* let go: a tap was a skip */
        if (!g_s.hold_seeked) {
            hm_music_skip(g_s.hold_dir);
            g_s.snd = HM_SND_MOVE;
        }
        g_s.hold_dir = 0;
    }
    g_v.mu_seek = g_s.hold_seeked && g_s.hold_dir ? g_s.hold_dir : 0;
    return ev & ~lr;
}

static void music_mode_toggle(int shuffle)
{
    hm_cfg *c = hm_cfg_get();

    if (shuffle)
        c->shuffle = !c->shuffle;
    else
        c->repeat = (c->repeat + 1) % 3;
    hm_cfg_save(HOME_CFG);
    hm_music_mode(c->shuffle, c->repeat);
    g_s.snd = HM_SND_MOVE;
}

/* The keys of the page shown (the overlays have theirs before). */
static void page_keys(unsigned int ev, unsigned int buttons, unsigned int now, int *nickname,
                      int *browser, int *start_usb)
{
    hm_page *p = &g_v.page[g_v.depth];
    int id = top_id(), modern = g_v.style == HM_STYLE_MODERN, cl = classic(),
        /* a list of pictures: along the row (Modern), down the column (the others) */
        back_one = modern ? PSP_CTRL_LEFT : PSP_CTRL_UP | (cl ? 0 : PSP_CTRL_LEFT),
        on_one = modern ? PSP_CTRL_RIGHT : PSP_CTRL_DOWN | (cl ? 0 : PSP_CTRL_RIGHT);
    int was_sel = p->sel, was_row = p->row_sel, depth = g_v.depth;

    /* the sections: L / R; in Classic also left / right on a section's first page */
    if (depth == 0 || !cl) {
        unsigned int prev_tab = PSP_CTRL_LTRIGGER | (cl && depth == 0 ? PSP_CTRL_LEFT : 0),
                     next_tab = PSP_CTRL_RTRIGGER | (cl && depth == 0 ? PSP_CTRL_RIGHT : 0);

        if (ev & (prev_tab | next_tab)) {
            open_tab((g_v.tab + (ev & prev_tab ? HM_TABS - 1 : 1)) % HM_TABS);
            g_s.snd = HM_SND_TAB;
            return;
        }
    }
    if ((ev & PSP_CTRL_LEFT) && cl && depth > 0 && id != P_NOW) { /* the XMB: left goes back */
        pop();
        g_s.snd = HM_SND_BACK;
        return;
    }
    if ((id == P_MUSIC && !cl) || id == P_NOW)
        ev = music_keys(ev, buttons, now);
    if ((ev & g_back) && depth > 0) {
        pop();
        g_s.snd = HM_SND_BACK;
        return;
    }
    switch (id) {
    case P_GAMES:
        p->sel = move_grid(p->sel, p->count, 1, ev, (unsigned int)back_one, (unsigned int)on_one);
        if ((ev & g_enter) && p->count) {
            launch(p->sel, 0, now);
        } else if ((ev & PSP_CTRL_SQUARE) && p->count) { /* a favourite, or not any more */
            int k = hm_library_toggle_fav(p->sel);
            hm_game *gm = hm_library_game(k);

            p->sel = k;
            p->selpos = k * 256;
            toast(pw_tr(gm && gm->fav ? PW_S_HM_FAV_ON : PW_S_HM_FAV_OFF), now);
            g_s.snd = HM_SND_OPEN;
            build_all();
        } else if ((ev & PSP_CTRL_TRIANGLE) && p->count) {
            push(P_GAME, p->sel);
            g_s.snd = HM_SND_OPEN;
        }
        break;
    case P_STATES:
    case P_SAVES:
        p->sel = move_grid(p->sel, p->count, 1, ev, PSP_CTRL_UP, PSP_CTRL_DOWN);
        if ((ev & g_enter) && p->count) {
            if (id == P_STATES) {
                hm_state *s = hm_pages_state(p->sel);

                if (s)
                    launch(g_stack[depth].arg, s->slot, now);
            } else {
                push(P_SAVE, p->sel);
                g_s.snd = HM_SND_OPEN;
            }
        } else if ((ev & (PSP_CTRL_SQUARE | PSP_CTRL_TRIANGLE)) && p->count) {
            g_s.confirm = id == P_STATES ? CONFIRM_STATE : CONFIRM_SAVE;
            g_s.confirm_arg = p->sel;
            g_s.snd = HM_SND_OPEN;
        }
        break;
    case P_ALBUMS:
        p->sel = move_grid(p->sel, p->count, modern ? HM_ALBUM_COLS : 1, ev, (unsigned int)back_one,
                           (unsigned int)on_one);
        if ((ev & g_enter) && p->count) {
            push(P_ALBUM, p->sel);
            g_s.snd = HM_SND_OPEN;
        }
        break;
    case P_ALBUM:
        p->sel = move_grid(p->sel, p->count, modern ? HM_PHOTO_COLS : 1, ev, (unsigned int)back_one,
                           (unsigned int)on_one);
        if ((ev & g_enter) && p->count) {
            g_s.viewer = 1;
            g_s.photo = hm_pages_album_photo(p->sel);
            g_s.t_caption = now;
            g_s.snd = HM_SND_OPEN;
        } else if ((ev & PSP_CTRL_SQUARE) && p->count) {
            g_s.confirm = CONFIRM_PHOTO;
            g_s.confirm_arg = hm_pages_album_photo(p->sel);
            g_s.snd = HM_SND_OPEN;
        }
        break;
    case P_NOW:
        if (ev & g_enter)
            hm_music_toggle(), g_s.snd = HM_SND_OPEN;
        else if (ev & PSP_CTRL_SQUARE)
            music_mode_toggle(1);
        else if (ev & PSP_CTRL_TRIANGLE)
            music_mode_toggle(0);
        else if (ev & PSP_CTRL_UP)
            hm_music_skip(-1), g_s.snd = HM_SND_MOVE;
        else if (ev & PSP_CTRL_DOWN)
            hm_music_skip(1), g_s.snd = HM_SND_MOVE;
        break;
    default: /* the rows */
        if (ev & PSP_CTRL_UP)
            p->row_sel = hm_row_next(p->rows, p->nrows, p->row_sel, -1);
        else if (ev & PSP_CTRL_DOWN)
            p->row_sel = hm_row_next(p->rows, p->nrows, p->row_sel, 1);
        else if (id == P_SET_CAT && !cl && (ev & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT))) {
            hm_row_act a = hm_page_act(g_stack[depth], p->row_sel);

            if (a.act == A_SETTING) {
                int was = pw_config_get()->sort, style = style_now();

                g_s.snd = HM_SND_MOVE;
                after_setting(hs_step(a.arg, ev & PSP_CTRL_LEFT ? -1 : 1), was, style);
            }
        } else if (id == P_MUSIC && (ev & PSP_CTRL_TRIANGLE)) {
            push(P_NOW, 0);
            g_s.snd = HM_SND_OPEN;
        } else if (id == P_MUSIC && (ev & PSP_CTRL_SQUARE)) {
            hm_music_stop();
            g_s.snd = HM_SND_BACK;
        } else if (ev & g_enter) {
            enter_row(p->row_sel, now, nickname, browser, start_usb);
        }
        break;
    }
    if (g_s.snd < 0 && (p->sel != was_sel || p->row_sel != was_row))
        g_s.snd = HM_SND_MOVE;
}

/* ---- the background of the page shown ---- */

static const unsigned int *page_background(void)
{
    int id = top_id(), cl = classic();
    hm_page *root = &g_v.page[0];

    if (g_s.editor && g_s.ed_bg)
        return g_s.ed_bg;
    if (g_s.viewer)
        return hm_library_photo_background(g_s.photo, 1);
    switch (id) {
    case P_GAMES:
        return hm_library_count() ? hm_library_background(root->sel) : 0;
    case P_GAME:
    case P_STATES:
    case P_INFO:
        return hm_library_background(g_stack[g_v.depth].arg);
    case P_SAVES:
    case P_SAVE: {
        const unsigned int *f = hm_library_file_background();

        return f ? f : hm_library_background(g_env.saves_game);
    }
    case P_MUSIC:
    case P_NOW:
        return cl ? g_gradient : hm_library_cover_background();
    case P_ALBUM:
        return cl ? g_gradient
                  : hm_library_photo_background(hm_pages_album_photo(g_v.page[g_v.depth].sel), 0);
    default:
        return g_gradient;
    }
}

/* What the loader works for, by the page shown. */
static void loader_focus(void)
{
    static int list[HM_PHOTOS_MAX];
    int id = top_id();
    hm_page *p = &g_v.page[g_v.depth];
    const hm_track *t = hm_music_track(hm_music_current());

    if (id == P_GAMES)
        hm_library_focus(p->sel);
    else if (id == P_GAME || id == P_STATES || id == P_INFO)
        hm_library_focus(g_stack[g_v.depth].arg);
    else if ((id == P_SAVES || id == P_SAVE) && g_env.saves_game >= 0)
        hm_library_focus(g_env.saves_game);
    if (id == P_ALBUM) {
        for (int k = 0; k < p->count && k < HM_PHOTOS_MAX; k++)
            list[k] = hm_pages_album_photo(k);
        hm_library_photo_view(list, p->count, p->sel);
        hm_library_mode(g_s.viewer ? 2 : 1, g_s.viewer ? g_s.photo : hm_pages_album_photo(p->sel));
    } else if (id == P_ALBUMS) {
        int k = 0;

        for (int a = 0; a < hm_library_album_count() && k < HM_PHOTOS_MAX; a++)
            if (hm_library_album(a)->count > 0)
                list[k++] = hm_library_album(a)->cover;
        hm_library_photo_view(list, k, p->sel);
        hm_library_mode(1, k ? list[p->sel < k ? p->sel : 0] : -1);
    } else {
        hm_library_mode(0, 0);
    }
    if (t && t->tagged) /* the cover of what plays (the tag's place known) */
        hm_library_cover(t->path, t->pic_off, t->pic_len);
    g_v.mu_cover = t ? hm_library_cover_pixels() : 0;
    if (id == P_SAVES || id == P_SAVE) { /* the save's PIC1 behind it */
        hm_save *s = hm_pages_save(id == P_SAVE ? g_stack[g_v.depth].arg : p->sel);

        if (id == P_SAVES)
            hm_library_pics_focus(p->sel);
        if (s) {
            char path[128];

            hm_saves_path(s, "PIC1.PNG", path, sizeof(path));
            hm_library_file_bg(path);
        }
    }
}

/* The hints of the page shown. */
static void page_hints(void)
{
    int id = top_id();
    hm_page *p = &g_v.page[g_v.depth];
    hm_row_act a = {A_NONE, 0, 0};

    if (p->nrows)
        a = hm_page_act(g_stack[g_v.depth], p->row_sel);
    g_v.hint_enter = g_v.hint_options = g_v.hint_square = 0;
    g_v.hint_back = g_v.depth > 0 ? pw_tr(PW_S_BACK) : 0;
    switch (id) {
    case P_GAMES:
        g_v.hint_enter = p->count ? pw_tr(PW_S_HM_PLAY) : 0;
        g_v.hint_options = p->count ? pw_tr(PW_S_HM_OPTIONS) : 0;
        g_v.hint_square = p->count ? pw_tr(PW_S_HM_FAVORITE) : 0;
        break;
    case P_STATES:
        g_v.hint_enter = p->count ? pw_tr(PW_S_HM_CONTINUE) : 0;
        g_v.hint_square = p->count ? pw_tr(PW_S_DELETE) : 0;
        break;
    case P_SAVES:
        g_v.hint_enter = p->count ? pw_tr(PW_S_HM_OPEN) : 0;
        g_v.hint_square = p->count ? pw_tr(PW_S_DELETE) : 0;
        break;
    case P_ALBUMS:
        g_v.hint_enter = p->count ? pw_tr(PW_S_HM_OPEN) : 0;
        break;
    case P_ALBUM:
        g_v.hint_enter = p->count ? pw_tr(PW_S_HM_VIEW) : 0;
        g_v.hint_square = p->count ? pw_tr(PW_S_DELETE) : 0;
        break;
    case P_MUSIC:
        if (hm_music_count())
            g_v.hint_enter = p->row_sel == 0 ? pw_tr(PW_S_HM_OPEN)
                             : p->row_sel - 1 == hm_music_current() && !hm_music_paused()
                                 ? pw_tr(PW_S_HM_PAUSE)
                                 : pw_tr(PW_S_HM_PLAY);
        g_v.hint_options = hm_music_count() ? pw_tr(PW_S_HM_NOW) : 0;
        g_v.hint_square = hm_music_current() >= 0 ? pw_tr(PW_S_HM_STOP) : 0;
        break;
    case P_NOW:
        g_v.hint_enter = hm_music_current() >= 0 && !hm_music_paused() ? pw_tr(PW_S_HM_PAUSE)
                                                                       : pw_tr(PW_S_HM_PLAY);
        g_v.hint_square = pw_tr(PW_S_HS_SHUFFLE);
        g_v.hint_options = pw_tr(PW_S_HS_REPEAT);
        break;
    default:
        if (a.act == A_OPEN || a.act == A_USB || a.act == A_BROWSER)
            g_v.hint_enter = pw_tr(PW_S_HM_OPEN);
        else if (a.act == A_SETTING && classic())
            g_v.hint_enter = pw_tr(PW_S_HM_CHANGE);
        else if (a.act == A_PLAY)
            g_v.hint_enter = pw_tr(PW_S_HM_PLAY);
        else if (a.act == A_DELETE)
            g_v.hint_enter = pw_tr(PW_S_DELETE);
        else if (a.act == A_NICKNAME || a.act == A_EXIT || a.act == A_SLEEP || a.act == A_OFF)
            g_v.hint_enter = pw_tr(PW_S_OK);
        break;
    }
    if (g_s.popup) {
        g_v.hint_enter = pw_tr(PW_S_OK);
        g_v.hint_back = pw_tr(PW_S_BACK);
    }
}

/* ---- deleting (a question first) ---- */

static void confirm_texts(void)
{
    static char text[96];

    g_v.sheet_busy = 0;
    g_v.sheet_glyph = HM_ICON_TRASH;
    g_v.sheet_title = pw_tr(PW_S_DELETE);
    g_v.sheet_hint = pw_tr(PW_S_HM_CANCEL);
    g_v.sheet_again = pw_tr(PW_S_DELETE);
    if (g_s.confirm == CONFIRM_STATE) {
        hm_state *s = hm_pages_state(g_s.confirm_arg);

        stbsp_snprintf(text, sizeof(text), pw_tr(PW_S_CONFIRM_DELETE), s ? s->slot : 0);
        g_v.sheet_text = text;
    } else {
        g_v.sheet_text =
            pw_tr(g_s.confirm == CONFIRM_SAVE ? PW_S_HM_DEL_SAVE_Q : PW_S_HM_DEL_PHOTO_Q);
    }
}

static void do_delete(unsigned int now)
{
    int ok = -1, id = top_id();

    if (g_s.confirm == CONFIRM_SAVE) {
        hm_save *s = hm_pages_save(g_s.confirm_arg);

        hm_library_pics(0, 0, 0);
        ok = s ? hm_saves_delete(s) : -1;
        hm_pages_saves(g_env.saves_game);
        if (id == P_SAVE)
            pop();
    } else if (g_s.confirm == CONFIRM_STATE) {
        hm_state *s = hm_pages_state(g_s.confirm_arg);
        hm_game *gm = hm_library_game(g_stack[g_v.depth].arg);

        ok = s && gm ? hm_saves_delete_state(gm->id, s->slot) : -1;
        hm_pages_states(g_stack[g_v.depth].arg);
        if (gm && ok == 0 && gm->states > 0) {
            gm->states--;
            hm_pages_texts();
        }
    } else if (g_s.confirm == CONFIRM_PHOTO) {
        hm_photo *ph = hm_library_photo(g_s.confirm_arg);

        hm_library_pause(1);
        ok = ph ? (sceIoRemove(ph->path) < 0 ? -1 : 0) : -1;
        hm_library_pause(0);
        if (ok == 0)
            hm_library_photo_removed(g_s.confirm_arg);
        hm_pages_album(-1);
        g_s.viewer = 0;
    }
    g_s.confirm = CONFIRM_NONE;
    toast(pw_tr(ok == 0 ? PW_S_HM_DELETED : PW_S_HM_DEL_FAIL), now);
    g_s.snd = ok == 0 ? HM_SND_BACK : HM_SND_ERROR;
    build_all();
}

/* The photo editor's keys; a save's slow part after a frame that says so. */
static void editor_keys(unsigned int ev, unsigned int now, unsigned int *t_prev)
{
    int changed = 0;

    if (ev & PSP_CTRL_LEFT) {
        g_v.ed_sel = g_v.ed_sel > 0 ? g_v.ed_sel - 1 : 0;
        g_s.snd = HM_SND_MOVE;
    } else if (ev & PSP_CTRL_RIGHT) {
        g_v.ed_sel = g_v.ed_sel < HM_ED_TOOLS - 1 ? g_v.ed_sel + 1 : g_v.ed_sel;
        g_s.snd = HM_SND_MOVE;
    } else if ((ev & (PSP_CTRL_UP | PSP_CTRL_DOWN)) && g_v.ed_sel != HM_ED_SAVE) {
        changed = hm_edit_step(&g_s.ed, g_v.ed_sel, ev & PSP_CTRL_UP ? 1 : -1);
    } else if ((ev & g_enter) && g_v.ed_sel != HM_ED_SAVE) {
        changed = hm_edit_step(&g_s.ed, g_v.ed_sel, 1);
    } else if ((ev & g_enter) && g_v.ed_sel == HM_ED_SAVE) {
        char path[200];
        int r;

        g_v.sheet = 256; /* one frame that says so before the slow part */
        g_v.sheet_busy = 1;
        g_v.sheet_title = pw_tr(PW_S_HM_ED_SAVE);
        g_v.sheet_text = 0;
        g_v.sheet_hint = g_v.sheet_again = 0;
        g_v.sheet_glyph = HM_ICON_CHECK;
        frame_now();
        r = hm_edit_changed(&g_s.ed) ? editor_save(path, sizeof(path)) : -1;
        g_v.sheet = 0;
        if (r == 0) {
            int k = hm_library_photo_added(path);

            editor_close();
            hm_pages_album(-1);
            build_all();
            if (k >= 0)
                g_s.photo = k;
            g_s.viewer = 0; /* back to the album, the copy first in it */
            toast(pw_tr(PW_S_HM_ED_SAVED), now);
            g_s.snd = HM_SND_OPEN;
        } else {
            toast(pw_tr(PW_S_HM_ED_FAILED), now);
            g_s.snd = HM_SND_ERROR;
        }
        *t_prev = hm_hw_now_ms();
    } else if (ev & g_back) {
        editor_close();
        g_s.snd = HM_SND_BACK;
    }
    if (changed) {
        editor_preview();
        g_s.snd = HM_SND_MOVE;
    }
}

/* A photo whole: left / right through the album, Enter edits, Triangle a slideshow, Square the
   wallpaper. */
static void viewer_keys(unsigned int ev, unsigned int now)
{
    hm_page *p = &g_v.page[g_v.depth];

    if ((ev & PSP_CTRL_LEFT) && p->sel > 0)
        p->sel--;
    else if ((ev & PSP_CTRL_RIGHT) && p->sel < p->count - 1)
        p->sel++;
    else if (ev & PSP_CTRL_TRIANGLE) /* a slideshow from here, or not */
        g_s.slideshow = !g_s.slideshow;
    else if ((ev & PSP_CTRL_SQUARE) && g_gradient && hm_library_photo(g_s.photo)) {
        const char *path = hm_library_photo(g_s.photo)->path;

        if (hm_library_wallpaper(path, g_gradient) == 0 && hs_wallpaper_set(path, g_gradient) == 0)
            toast(pw_tr(PW_S_HM_WP_DONE), now);
        make_default_background(); /* (the old one when that failed) */
    } else if (ev & g_enter) {
        if (editor_open() == 0) {
            g_s.slideshow = 0;
            g_s.snd = HM_SND_OPEN;
        } else {
            g_s.snd = HM_SND_ERROR; /* (the picture not loaded yet) */
        }
    } else if (ev & g_back) {
        g_s.viewer = g_s.slideshow = 0;
        g_s.snd = HM_SND_BACK;
    }
    g_s.photo = hm_pages_album_photo(p->sel);
    if (ev & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT | PSP_CTRL_TRIANGLE | PSP_CTRL_SQUARE)) {
        g_s.t_caption = now;
        g_s.t_slide = now;
        if (g_s.snd < 0)
            g_s.snd = HM_SND_MOVE;
    }
}

/* The HOME panel's keys. */
static void panel_keys(unsigned int ev, int *start_usb)
{
    if (ev & PSP_CTRL_UP) {
        g_v.panel_sel = g_v.panel_sel > 0 ? g_v.panel_sel - 1 : 0;
        g_s.snd = HM_SND_MOVE;
    } else if (ev & PSP_CTRL_DOWN) {
        g_v.panel_sel = g_v.panel_sel < g_v.panel_count - 1 ? g_v.panel_sel + 1 : g_v.panel_sel;
        g_s.snd = HM_SND_MOVE;
    } else if (ev & (g_back | PSP_CTRL_HOME)) {
        g_v.panel_count = 0;
        g_s.snd = HM_SND_BACK;
    } else if (ev & g_enter) {
        g_v.panel_count = 0;
        if (g_v.panel_sel == PANEL_USB)
            *start_usb = 1;
        else if (g_v.panel_sel == PANEL_XMB)
            g_s.leave = 1;
        else if (g_v.panel_sel == PANEL_SLEEP)
            scePowerRequestSuspend();
        else if (g_v.panel_sel == PANEL_OFF)
            scePowerRequestStandby();
        g_s.snd = HM_SND_OPEN;
    }
}

/* Classic: a choice's options. */
static void popup_keys(unsigned int ev)
{
    if (ev & PSP_CTRL_UP) {
        g_v.popup_sel = g_v.popup_sel > 0 ? g_v.popup_sel - 1 : 0;
        g_s.snd = HM_SND_MOVE;
    } else if (ev & PSP_CTRL_DOWN) {
        g_v.popup_sel = g_v.popup_sel < g_v.popup_count - 1 ? g_v.popup_sel + 1 : g_v.popup_sel;
        g_s.snd = HM_SND_MOVE;
    } else if (ev & g_enter) {
        int was = pw_config_get()->sort, style = style_now();

        g_s.popup = 0;
        g_s.snd = HM_SND_OPEN;
        after_setting(hs_choose(g_s.popup_row, g_v.popup_sel), was, style);
    } else if (ev & (g_back | PSP_CTRL_LEFT)) {
        g_s.popup = 0;
        g_s.snd = HM_SND_BACK;
    }
}

/* After USB: the card may have changed - everything read again. */
static void usb_end(void)
{
    hm_usb_stop();
    usb_log(); /* what each step did, and the states the link went through */
    g_s.usb = 0;
    hm_pages_drop();
    hm_library_rescan();
    hm_pages_texts();
    hm_library_count_free();
    hm_library_pause(0);
    hm_music_pause_tags(0);
    open_tab(g_v.tab);
    build_all();
    g_s.snd = HM_SND_BACK;
}

static void usb_begin(unsigned int now)
{
    hm_music_stop(); /* the loader, the tags and the music leave the card first */
    hm_music_pause_tags(1);
    hm_library_pause(1);
    g_s.usb = 1;
    g_s.usb_state = hm_usb_start(now);
    usb_log(); /* the drivers: before the connection, the card is still ours */
    if (g_s.usb_state != HM_USB_FAILED)
        g_s.usb_state = hm_usb_poll(now);
    g_s.t_usb = now;
}

static void launch_now(stats *st, unsigned int now)
{
    hm_game *gm = hm_library_game(g_s.launching);

    stats_write(st, now, "before a game");
    hm_music_stop();
    hm_sound_end();
    if (g_s.launch_slot > 0 && hm_states_continue(gm->id, g_s.launch_slot) < 0)
        g_s.launch_slot = 0; /* continue.txt not written: the game starts plainly */
    if (gm->kind == HM_KIND_ISO)
        hm_launch(gm->path, hm_library_has(gm->path, "PSP_GAME/SYSDIR/EBOOT.OLD"));
    else if (gm->kind == HM_KIND_PSN)
        hm_launch_psn(gm->path);
    else
        hm_launch_eboot(gm->path, gm->kind == HM_KIND_PS1);
    if (g_s.launch_slot > 0) /* it returned: the start failed; no state for a later start */
        hm_hw_remove(CONTINUE);
    g_s.launching = -1; /* the start failed: Home again */
    g_s.t_open = now;
    hm_sound_init();
    hm_sound_enable(hs_sounds());
}

/* HOLD turns the screen off (hcfg.h hold): after half a second of HOLD, while music plays (or
   always); the backlight and the CPU's clock come back when HOLD goes off. 1 while it is off. */
static int hold_screen(unsigned int buttons, unsigned int now)
{
    int mode = hm_cfg_get()->hold, hold = (buttons & PSP_CTRL_HOLD) != 0,
        want = hold && !g_s.usb && g_s.launching < 0 &&
               (mode == HM_HOLD_ALWAYS ||
                (mode == HM_HOLD_MUSIC && hm_music_current() >= 0 && !hm_music_paused()));

    if (!hold)
        g_s.t_hold = 0;
    else if (!g_s.t_hold)
        g_s.t_hold = now | 1;
    if (want && !g_s.screen_off && now - g_s.t_hold >= 500) {
        if (hm_hw_backlight(0) == 0) {
            g_s.screen_off = 1;
            g_s.cpu_mhz = scePowerGetCpuClockFrequencyInt();
            if (g_s.cpu_mhz > 222) /* (the music is the Media Engine's: the CPU can rest) */
                scePowerSetClockFrequency(222, 222, 111);
        }
    } else if (!want && g_s.screen_off) {
        if (g_s.cpu_mhz > 222)
            scePowerSetClockFrequency(g_s.cpu_mhz, g_s.cpu_mhz, g_s.cpu_mhz / 2);
        hm_hw_backlight(1);
        g_s.screen_off = 0;
    }
    return g_s.screen_off;
}

/* ---- the loop ---- */

int main(void)
{
    pw_keys keys = {0, 0, 0};
    stats st;
    unsigned int t_prev, t_rows = 0, t_status = 0;
    const unsigned int *shown = 0; /* the background drawn last */
    int up = 0;
    char clock[24] = "";

    if (hm_hw_init() < 0)
        return 0;
    pw_icon_cache(malloc(64 * 1024), 64 * 1024);
    load_fonts();
    g_ui.cross_enter = hm_hw_cross_enter();
    g_enter = g_ui.cross_enter ? PSP_CTRL_CROSS : PSP_CTRL_CIRCLE;
    g_back = g_ui.cross_enter ? PSP_CTRL_CIRCLE : PSP_CTRL_CROSS;
    hm_texts_load();
    pw_config_load(PW_DIR "config.txt");
    hm_cfg_load(HOME_CFG);
    apply_look();
    hm_hw_model(g_model, sizeof(g_model));
    hm_hw_firmware(g_firmware, sizeof(g_firmware));
    hm_hw_cfw(g_cfw, sizeof(g_cfw));
    hm_hw_mac(g_mac, sizeof(g_mac));
    g_env.model = g_model;
    g_env.firmware = g_firmware;
    g_env.cfw = g_cfw;
    g_env.mac = g_mac;
    g_env.saves_game = -1;
    hm_sound_init();
    hm_sound_enable(hs_sounds());
    load_art();
    g_v.style = style_now();
    hm_library_style(g_v.style == HM_STYLE_CLASSIC);
    g_gradient = memalign(64, 512 * HM_H * 4);
    make_default_background();
    hm_library_sort(pw_config_get()->sort); /* (no games yet: only the order to use) */
    hm_library_scan();
    hm_pages_texts();
    hm_library_start();
    hm_music_scan();
    hm_music_mode(hm_cfg_get()->shuffle, hm_cfg_get()->repeat);

    g_v.clock = clock;
    g_v.bg = g_gradient;
    g_v.bg_mix = 256;
    g_v.mu_cur = -1;
    g_s.launching = -1;
    texts(&g_v);
    open_tab(HM_TAB_GAMES);
    build_all();
    shown = g_gradient;
    g_s.t_open = t_prev = hm_hw_now_ms();
    memset(&st, 0, sizeof(st));
    st.t0 = st.last = g_s.t_open;

    while (!hm_hw_exit_requested() && !g_s.leave) {
        unsigned int now = hm_hw_now_ms(), t_cpu = sceKernelGetSystemTimeLow(), t_warm,
                     buttons = hm_hw_buttons(),
                     ev = pw_keys_update(&keys, buttons, REPEAT, now * 1000);
        const unsigned int *want;
        int dt = (int)(now - t_prev), start_usb = 0, nickname = 0, browser = 0,
            tau = hm_cfg_tau(hm_cfg_get()->motion, PW_ANIM_TAU_MS), was_depth = g_v.depth,
            was_tab = g_v.tab;
        hm_gfx *g;

        t_prev = now;
        g_s.snd = -1;
        /* the sleep timer: the music stops and the PSP sleeps */
        if (hs_timer_end() && (int)(now - hs_timer_end()) >= 0) {
            hs_timer_done();
            hm_music_stop();
            if (g_s.screen_off)
                hm_hw_backlight(1), g_s.screen_off = 0;
            scePowerRequestSuspend();
        }
        /* HOLD: the screen off (while music plays, or always), the CPU slower; nothing drawn */
        if (hold_screen(buttons, now)) {
            sceKernelDelayThread(50 * 1000);
            t_prev = hm_hw_now_ms();
            continue;
        }
        /* input: USB, a question, the panels, the editor, the viewer, then the page */
        if (g_s.usb) {
            if ((ev & g_enter) && g_s.usb_state != HM_USB_CONNECTED) { /* again, by hand */
                if (g_s.usb_state == HM_USB_FAILED) {
                    hm_usb_stop();
                    start_usb = 1;
                } else {
                    g_s.usb_state = hm_usb_again(now);
                }
            } else if (ev & (g_back | PSP_CTRL_HOME)) { /* the card is ours again */
                usb_end();
                t_rows = 0;
                shown = 0; /* the background again for what is now chosen */
            }
        } else if (g_s.confirm) {
            if (ev & g_enter) {
                do_delete(now);
            } else if (ev & (g_back | PSP_CTRL_HOME)) {
                g_s.confirm = CONFIRM_NONE;
                g_s.snd = HM_SND_BACK;
            }
        } else if (g_s.launching < 0 && g_v.panel_count) {
            panel_keys(ev, &start_usb);
        } else if (g_s.launching < 0 && (ev & PSP_CTRL_HOME)) {
            panel_home(&g_v);
            g_v.panel_sel = 0;
            g_s.snd = HM_SND_OPEN;
        } else if (g_s.launching < 0 && g_s.popup) {
            popup_keys(ev);
        } else if (g_s.launching < 0 && g_s.editor) {
            editor_keys(ev, now, &t_prev);
        } else if (g_s.launching < 0 && g_s.viewer) {
            viewer_keys(ev, now);
        } else if (g_s.launching < 0) {
            page_keys(ev, buttons, now, &nickname, &browser, &start_usb);
        }
        if (start_usb || nickname || browser)
            g_s.snd = HM_SND_OPEN;
        if (ev && g_s.snd >= 0) /* the key sounds (HOM-REQ-019) */
            hm_sound_play(g_s.snd);
        if (start_usb)
            usb_begin(now);
        if (g_s.viewer && g_s.slideshow &&
            now - g_s.t_slide >= k_slide_ms[pw_config_get()->slide % 3]) {
            hm_page *p = &g_v.page[g_v.depth];

            g_s.t_slide = now; /* the next, round again */
            p->sel = p->count > 0 ? (p->sel + 1) % p->count : 0;
            g_s.photo = hm_pages_album_photo(p->sel);
        }
        if (g_s.usb && now - g_s.t_usb >= 250) {
            g_s.t_usb = now;
            g_s.usb_state = hm_usb_poll(now);
        }
        if (g_s.usb)
            usb_texts(&g_v, g_s.usb_state,
                      g_s.usb_state == HM_USB_FAILED || hm_usb_may_retry(g_s.usb_state, now));
        else if (g_s.confirm)
            confirm_texts();
        /* the firmware's own dialogs, over Home's screen */
        if (browser) {
            run_browser();
            t_rows = 0;
            t_prev = hm_hw_now_ms();
            continue;
        }
        if (nickname) {
            char was[64], typed[64];

            hs_nickname(was, sizeof(was));
            if (hm_dialog_osk(pw_tr(PW_S_HS_NICKNAME), was, typed, sizeof(typed), g_ui.cross_enter,
                              hm_hw_language(), dialog_frame, &g_v) == 1)
                hs_set_nickname(typed);
            t_rows = 0;
            t_prev = hm_hw_now_ms();
            continue;
        }
        /* a page opened or a section changed: its rows now; whose saves are listed */
        if (g_v.depth != was_depth || g_v.tab != was_tab)
            t_rows = 0;
        for (int d = 0; d <= g_v.depth; d++)
            if (g_stack[d].id == P_GAME)
                g_env.saves_game = g_stack[d].arg;
        /* what changes by the second: the clock, the battery, the pages' rows */
        if (t_status == 0 || now - t_status >= 1000) {
            hm_battery b;

            t_status = now;
            hm_hw_clock(clock, sizeof(clock), hm_cfg_get()->clock);
            hm_hw_battery(&b);
            g_v.battery = b.pct;
            g_v.charging = b.charging;
        }
        {
            unsigned int t0 = sceKernelGetSystemTimeLow(), us;
            int id = top_id();

            if (t_rows == 0 || now - t_rows >= 1000) {
                build_all();
                t_rows = now;
            } else if (id == P_MUSIC || id == P_NOW || id == P_SAVES || id == P_ALBUM ||
                       id == P_ALBUMS || id == P_GAMES) {
                /* the time playing, the pictures arriving: the page shown only */
                hm_page_build(&g_v, g_v.depth, g_stack[g_v.depth], &g_env);
            }
            us = sceKernelGetSystemTimeLow() - t0;
            if (us > st.rows_us)
                st.rows_us = us;
        }
        g_v.battery_pct = hm_cfg_get()->percent;
        page_hints();
        loader_focus();
        /* motion, by time */
        for (int d = 0; d < HM_DEPTH; d++) {
            hm_page *p = &g_v.page[d];

            p->selpos = pw_ease(p->selpos, p->sel * 256, dt, tau, 6);
            p->row_pos = pw_ease(p->row_pos, p->row_sel * 256, dt, tau, 6);
        }
        g_v.depth_pos = pw_ease(g_v.depth_pos, g_v.depth * 256, dt, tau * 3 / 2, 6);
        g_v.tab_pos = pw_ease(g_v.tab_pos, g_v.tab * 256, dt, tau, 6);
        g_v.panel = pw_ease(g_v.panel, g_v.panel_count ? 256 : 0, dt, tau, 6);
        g_v.panel_pos = pw_ease(g_v.panel_pos, g_v.panel_sel * 256, dt, tau, 6);
        g_v.sheet = pw_ease(g_v.sheet, g_s.usb || g_s.confirm ? 256 : 0, dt, tau, 6);
        g_v.popup = pw_ease(g_v.popup, g_s.popup ? 256 : 0, dt, tau, 6);
        g_v.popup_pos = pw_ease(g_v.popup_pos, g_v.popup_sel * 256, dt, tau, 6);
        g_v.editor = pw_ease(g_v.editor, g_s.editor ? 256 : 0, dt, tau, 6);
        g_v.ed_pos = pw_ease(g_v.ed_pos, g_v.ed_sel * 256, dt, tau, 6);
        g_v.toast =
            now - g_s.t_toast < TOAST_MS ? 256 : 256 - pw_ramp(now, g_s.t_toast + TOAST_MS, 300);
        if (g_s.editor)
            editor_texts();
        /* a photo whole: its name and the buttons 2 s after a change, then they fade out */
        g_v.viewer = g_s.viewer || g_s.editor ? 256 : 0;
        g_v.viewer_caption =
            g_s.viewer && hm_library_photo(g_s.photo) ? hm_library_photo(g_s.photo)->name : 0;
        g_v.viewer_slideshow = pw_tr(PW_S_HM_SLIDESHOW);
        g_v.viewer_wallpaper = pw_tr(PW_S_HM_WP_SET);
        g_v.viewer_edit = pw_tr(PW_S_HM_EDIT);
        g_v.caption =
            now - g_s.t_caption < 2000 ? 256 : 256 - pw_ramp(now, g_s.t_caption + 2000, 400);
        /* the page's background (a game's art, a cover, a photo), faded in once it is made */
        want = page_background();
        if (!want)
            want = g_s.viewer ? shown : g_gradient;
        if (want != shown && want) {
            g_v.bg_prev = shown;
            g_v.bg = want;
            g_v.bg_mix = shown ? 0 : 256;
            shown = want;
        }
        g_v.bg_mix = g_v.bg_mix + dt * 256 / MIX_MS > 256 ? 256 : g_v.bg_mix + dt * 256 / MIX_MS;
        /* the XMB's wave over its sky, fading with it */
        g_v.wave = !g_sky_shown || !hm_cfg_get()->wave ? 0
                   : g_v.bg == g_gradient      ? (g_v.bg_prev == g_gradient ? 256 : g_v.bg_mix)
                   : g_v.bg_prev == g_gradient ? 256 - g_v.bg_mix
                                               : 0;
        g_v.aurora = !g_aurora_shown             ? 0
                     : g_v.bg == g_gradient      ? (g_v.bg_prev == g_gradient ? 256 : g_v.bg_mix)
                     : g_v.bg_prev == g_gradient ? 256 - g_v.bg_mix
                                                 : 0;
        g_v.t_ms = now;
        g_v.appear = g_s.launching >= 0 ? 256 - pw_ramp(now, g_s.t_fade, FADE_MS)
                                        : pw_ramp(now, g_s.t_open, FADE_MS);

        /* the frame: the list, then - while the GE draws it - what the next frames may need */
        g = hm_gu_begin();
        hm_draw(g, &g_ui, &g_v);
        hm_gu_finish();
        t_warm = sceKernelGetSystemTimeLow();
        t_cpu = t_warm - t_cpu;
        hm_prefetch(g, &g_ui, &g_v);
        t_warm = sceKernelGetSystemTimeLow() - t_warm;
        stats_frame(&st, now, t_cpu, t_warm, hm_gu_end());
        if (!up) { /* the first screen is up: tell the XMB plugin's guard */
            hm_hw_remove(PENDING);
            up = 1;
        }
        if (!g_s.usb && now - st.t0 >= STATS_MS)
            stats_write(&st, now, "running");
        if (g_s.launching >= 0 && now - g_s.t_fade >= FADE_MS)
            launch_now(&st, now);
    }
    if (g_s.usb) {
        hm_usb_stop();
        usb_log();
    }
    if (g_s.screen_off) /* (never left dark) */
        hm_hw_backlight(1);
    stats_write(&st, hm_hw_now_ms(), "leaving");
    hm_sound_end();
    hm_hw_touch(XMB_ONCE); /* leaving Home means the XMB, even when Home is the start screen */
    sceKernelExitGame();
    return 0;
}
