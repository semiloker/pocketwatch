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

#include "core/battmodel.h"
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
#include "keyboard.h"
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
#define HOME_CFG  PW_DIR "home.txt"     /* Home's own settings (hcfg.h) */
#define HOME_SELF "ms0:/PSP/GAME/PocketwatchHome/EBOOT.PBP" /* Home itself (deploy's copy) */
#define BROWSER   PW_DIR "browser.txt" /* the browser's first page (a URL), if not the default */
/* A search made for old browsers (plain HTTP; the PSP's browser has no modern TLS). PROPOSED. */
#define BROWSER_HOME  "http://frogfind.com/"
#define STATS_MS      30000
#define FADE_MS       250
#define MIX_MS        300 /* a new background fades in */
#define INTRO_HOLD_MS 650 /* the start-up wordmark holds, then fades out over INTRO_FADE_MS */
#define INTRO_FADE_MS 450
#define TOAST_MS      1800
#define HOLD_MS       450 /* left / right held this long: seeking on by steps */
#define SEEK_EVERY    220
#define SEEK_STEP     5    /* s */
#define SEEK_TAP      10   /* s: one press of left / right */
#define FLASH_MS      800  /* the ring on the player's button a key just worked */
#define EDIT_SIDE     1600 /* an edited copy's longest side at most (memory) */
#define REPEAT        (PSP_CTRL_LEFT | PSP_CTRL_RIGHT | PSP_CTRL_UP | PSP_CTRL_DOWN)

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
    if (ok) { /* the PSP's font, as Home draws it (the previews and PPSSPP have another one) */
        char line[200];
        int w[3], l[3], adv[3];
        const char *probe = "Ul";

        for (int i = 0; i < 2; i++)
            adv[i] = pw_font_glyph_metrics(&g_fonts[0], (unsigned char)probe[i], &w[i], &l[i]);
        stbsp_snprintf(line, sizeof(line),
                       "home font: %s rev %d, capitals %d / %d / %d px (title, body, big); title: "
                       "U adv %d/64 w %d left %d, l adv %d/64 w %d left %d\n",
                       FONT_PATH, pgf.revision, pw_font_cap(g_ui.title), pw_font_cap(g_ui.body),
                       pw_font_cap(g_ui.big), adv[0], w[0], l[0], adv[1], w[1], l[1]);
        hm_hw_log(line);
    }
}

/* ---- the frame rate (HOM-NFR-001): home.log, so a slow screen shows in numbers ---- */

typedef struct {
    unsigned int t0, frames, cpu_us, ge_us, warm_us, worst_ms, last, late, made, rows_us;
    char slow[400]; /* the frames over SLOW_MS: on which page, and where their time went */
} stats;

#define SLOW_MS 100

/* Where the frame's time went, by part (us): the keys, the rows, the rest of the page's state (the
   loader's focus, the motion), the drawing (the CPU's list), the prefetch; set by the loop. */
static unsigned int g_t_keys, g_t_rows, g_t_state, g_t_draw, g_t_warm;

static void stats_frame(stats *st, unsigned int now, unsigned int cpu_us, unsigned int warm_us,
                        unsigned int ge_us, int page)
{
    unsigned int made, kept, bytes;

    unsigned int took = hm_hw_now_ms() - now; /* this frame, from its keys to its end */
    int n = (int)strlen(st->slow);

    hm_gu_stats(&made, &kept, &bytes);
    if (st->frames) {
        if (now - st->last > st->worst_ms)
            st->worst_ms = now - st->last;
        if (now - st->last > 20) /* more than one vblank: a frame missed */
            st->late++;
    }
    /* a stall the owner sees (2026-10-20: "micro freezes" in Photos): what it was, so the next
       round names it */
    if (took >= SLOW_MS && n < (int)sizeof(st->slow) - 64)
        stbsp_snprintf(st->slow + n, sizeof(st->slow) - (size_t)n,
                       " [%u ms p%d keys %u rows %u state %u draw %u warm %u ge %u st %u]", took,
                       page, g_t_keys / 1000, g_t_rows / 1000, g_t_state / 1000, g_t_draw / 1000,
                       g_t_warm / 1000, ge_us / 1000, made);
    st->last = now;
    st->frames++;
    st->cpu_us += cpu_us;
    st->warm_us += warm_us;
    st->ge_us += ge_us;
    st->made += made;
}

/* What the battery's calls cost: once, so a slow one shows by name. */
static int g_costs_written;

static void stats_write(stats *st, unsigned int now, const char *why)
{
    char line[720];
    unsigned int ms = now - st->t0, n = st->frames ? st->frames : 1;

    if (st->frames < 2 || ms == 0)
        return;
    if (!g_costs_written) {
        char cost[200];

        if (hm_hw_battery_cost(cost, sizeof(cost))) {
            stbsp_snprintf(line, sizeof(line), "home reads: battery: %s\n", cost);
            hm_hw_log(line);
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
                       "(%u KB); heap %u of %u KB, outside %u KB%s%s\n",
                       why, st->frames, ms, st->frames * 1000 / ms, st->late, st->cpu_us / n,
                       st->warm_us / n, st->ge_us / n, st->worst_ms, st->rows_us, st->made, kept,
                       bytes >> 10, (unsigned int)mi.uordblks >> 10, (unsigned int)mi.arena >> 10,
                       (unsigned int)sceKernelMaxFreeMemSize() >> 10, st->slow[0] ? "; slow:" : "",
                       st->slow);
    }
    hm_hw_log(line);
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
    hm_hw_log(line);
}

/* ---- the HOME panel (HOM-REQ-010) ---- */

enum { PANEL_BRIGHT, PANEL_USB, PANEL_XMB, PANEL_SLEEP, PANEL_RESTART, PANEL_OFF, PANEL_ITEMS };

/* A picture's time in the slideshow by Settings -> Slideshow Speed: normal, fast, slow
   (INFERRED from the XMB's Photo Settings; its exact times are not documented). */
static const unsigned int k_slide_ms[3] = {5000, 3000, 10000};

static void panel_home(hm_view *v)
{
    static char blv[8];
    int bl = hm_bright_get();

    memset(v->panel_values, 0, sizeof(v->panel_values));
    memset(v->panel_icons, 0, sizeof(v->panel_icons));
    memset(v->panel_glyph, 0, sizeof(v->panel_glyph));
    for (int i = 0; i < HM_PANEL_MAX; i++)
        v->panel_slider[i] = -1;
    v->panel_title = "Pocketwatch";
    v->panel_items[PANEL_BRIGHT] = pw_tr(PW_S_HM_ED_LIGHT); /* "Brightness" */
    v->panel_items[PANEL_USB] = pw_tr(PW_S_HM_USB);
    v->panel_items[PANEL_XMB] = pw_tr(PW_S_HM_EXIT);
    v->panel_items[PANEL_SLEEP] = pw_tr(PW_S_HM_SLEEP);
    v->panel_items[PANEL_RESTART] = pw_tr(PW_S_HM_RESTART);
    v->panel_items[PANEL_OFF] = pw_tr(PW_S_HM_POWER_OFF);
    v->panel_glyph[PANEL_BRIGHT] = HM_ICON_SUN;
    v->panel_glyph[PANEL_USB] = HM_ICON_USB;
    v->panel_glyph[PANEL_XMB] = HM_ICON_EXIT;
    v->panel_glyph[PANEL_SLEEP] = HM_ICON_SLEEP;
    v->panel_glyph[PANEL_RESTART] = HM_ICON_ROTATE;
    v->panel_glyph[PANEL_OFF] = PW_ICON_QUIT;
    int bmax = hm_bright_max();

    v->panel_slider[PANEL_BRIGHT] = bl >= 0 && bmax > 0 ? bl * 256 / bmax : -1;
    if (bl >= 0) { /* Classic and Minimal draw the level as text (no slider) */
        stbsp_snprintf(blv, sizeof(blv), "%d/%d", bl + 1, bmax + 1);
        v->panel_values[PANEL_BRIGHT] = blv;
    }
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
    } else if (bg == HM_BG_NEBULA) { /* a starry night sky with wave ribbons */
        hm_make_nebula(g_ui.accent, g_gradient);
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
static char g_art_reads[320];

static unsigned char *read_whole(const char *path, unsigned int *len)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    int size = fd >= 0 ? (int)sceIoLseek32(fd, 0, PSP_SEEK_END) : fd, got = 0, ksize = 0;
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
    /* refused to this program (0x8001000D: topmenu_icon.rco, the sky; the game plugin reads
       them): the kernel reads it */
    if (fd < 0 && fd != (int)0x80010002) {
        ksize = hm_hw_kernel_size(path);
        if (ksize > 0 && ksize <= 512 * 1024 && (buf = malloc((size_t)ksize)) != 0 &&
            (got = hm_hw_kernel_read(path, buf, (unsigned int)ksize)) != ksize) {
            free(buf);
            buf = 0;
        }
        if (buf)
            size = ksize;
    }
    stbsp_snprintf(g_art_reads + n, (int)sizeof(g_art_reads) - n, " %s %X", name ? name + 1 : path,
                   (unsigned int)fd >= 0x80000000u ? (unsigned int)fd : (unsigned int)size);
    n = (int)strlen(g_art_reads);
    stbsp_snprintf(g_art_reads + n, (int)sizeof(g_art_reads) - n, "%s%s",
                   ksize ? (buf ? " kernel ok" : " kernel refused") : "",
                   buf       ? ""
                   : got < 0 ? " read failed"
                             : " not taken");
    if (ksize && !buf) {
        n = (int)strlen(g_art_reads);
        stbsp_snprintf(g_art_reads + n, (int)sizeof(g_art_reads) - n, " %X",
                       (unsigned int)(got < 0 ? got : ksize));
    }
    *len = buf ? (unsigned int)size : 0;
    return buf;
}

static void load_art(void)
{
    unsigned int t0 = sceKernelGetSystemTimeLow(), mask = 0;
    int colour, sky = hm_hw_sky_index(&colour), found, items = 0;
    char line[480];
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
    hm_hw_log(line);
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
    hm_hw_log(line);
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
    v->mu_now_label = pw_tr(PW_S_HM_NOW);
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
static void now_opened(void);

static void push(int id, int arg)
{
    int d = g_v.depth + 1;

    if (d >= HM_DEPTH)
        return;
    g_stack[d].id = id;
    g_stack[d].arg = arg;
    if (id == P_NOW)
        now_opened();
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
    g_v.page[0].opt_sel = g_v.page[0].opt_focus =
        0; /* a fresh section: no stale two-pane/dash focus */
    g_v.page[0].opt_pos = g_v.page[0].opt_focus_pos = 0;
    if (g_v.page[0].rows)
        first_row(0);
}

static int top_id(void)
{
    return g_stack[g_v.depth].id;
}

/* ---- the loop's state ---- */

enum { CONFIRM_NONE, CONFIRM_SAVE, CONFIRM_STATE, CONFIRM_PHOTO, CONFIRM_WIFI, CONFIRM_CAL };
enum { KB_NICKNAME, KB_WIFI }; /* what the keyboard is typing */

static struct {
    int launching, launch_slot, leave, usb, usb_state, popup, popup_row;
    int viewer, slideshow, photo; /* the photo shown whole (its index) */
    int confirm, confirm_arg;
    int hold_dir; /* left / right held on Now Playing: seeking */
    unsigned int hold_t0, hold_last;
    int mu_flash;                     /* Now Playing: the button a key just worked, -1 */
    unsigned int mu_flash_t, mu_keys; /* when; the buttons down at its last frame */
    unsigned int t_fade, t_open, t_caption, t_slide, t_toast, t_usb;
    int editor, ed_dirty;
    hm_edit ed;
    unsigned int *ed_src, *ed_bg; /* the picture fitted to the screen; the preview drawn */
    int ed_w, ed_h;
    int snd;
    int panel_open;      /* the HOME panel (its items stay while it fades out) */
    int screen_off;      /* HOLD: the backlight off (music on) */
    unsigned int t_hold; /* since when HOLD is on */
    int cpu_mhz;         /* the clock before the screen went off */
    int kb, kb_saved;    /* Home's keyboard is open (the nickname); it saved */
    int kb_for;          /* KB_*: the nickname, a Wi-Fi password */
} g_s;

static hm_kb g_kb;

static void toast(const char *text, unsigned int now)
{
    g_v.toast_text = text;
    g_s.t_toast = now;
}

/* ---- the network's jobs (net.c: a thread of their own, the screen goes on): the clock from the
   internet (HOM-REQ-032, ADR-041), Wi-Fi (HOM-REQ-033, ADR-044) ---- */

static hm_net_result g_net; /* the job running, else the last one's answer */
static hm_wifi_ap g_kb_ap;  /* the network a password is typed for */

static void net_start(int kind, int slot, const hm_wifi_ap *ap, const char *key)
{
    if (hm_net_job(kind, slot, ap, key) < 0)
        return;
    if (kind == HM_JOB_TIME)
        hs_set_net_time(HS_NET_ASKING, 0);
    g_env.net_job = kind;
    g_env.net = &g_net;
    g_net.kind = kind;
    g_net.slot = slot;
}

/* A job's answer, once: the rows' values and a toast. */
static void net_poll(unsigned int now)
{
    static char text[80];
    int st = hm_net_job_poll(&g_net), r = g_net.ret;
    char when[16];

    g_env.net = &g_net;
    g_env.net_job = st == 1 ? g_net.kind : HM_JOB_NONE;
    if (st != 2)
        return;
    hm_hw_clock(when, sizeof(when), HM_CLOCK_TIME);
    if (g_net.kind == HM_JOB_TIME || g_net.time_set)
        hs_set_net_time(r == 0 || g_net.time_set ? HS_NET_DONE
                        : r == HM_NET_SWITCH_OFF ? HS_NET_NO_WIFI
                        : r == HM_NET_NO_PROFILE ? HS_NET_NO_PROFILE
                                                 : HS_NET_FAILED,
                        when);
    if (g_net.kind == HM_JOB_TIME) {
        toast(pw_tr(r == 0                   ? PW_S_CLK_SYNCED
                    : r == HM_NET_SWITCH_OFF ? PW_S_CLK_NO_WIFI
                    : r == HM_NET_NO_PROFILE ? PW_S_CLK_NO_NET
                                             : PW_S_CLK_FAILED),
              now);
    } else if (g_net.kind == HM_JOB_SCAN) {
        if (r == HM_NET_SWITCH_OFF)
            toast(pw_tr(PW_S_CLK_NO_WIFI), now);
    } else { /* a connection made or tried */
        hm_pages_wifi_read();
        if (r == 0)
            stbsp_snprintf(text, sizeof(text), pw_tr(PW_S_WIFI_CONNECTED), g_net.ip);
        toast(r == 0                   ? text
              : r == HM_NET_SWITCH_OFF ? pw_tr(PW_S_CLK_NO_WIFI)
              : r == HM_NET_NOT_SAVED  ? pw_tr(PW_S_WIFI_NOT_SAVED)
              : r == HM_NET_FULL       ? pw_tr(PW_S_WIFI_FULL)
                                       : pw_tr(PW_S_WIFI_FAILED),
              now);
    }
}

/* Enter on a network found: a new connection - its password typed first. */
static void wifi_join(int i, unsigned int now)
{
    const hm_wifi_ap *ap = hm_pages_wifi_ap(i);
    static char title[96];

    if (!ap)
        return;
    if (ap->security == HM_WIFI_SEC_NONE) {
        net_start(HM_JOB_JOIN, 0, ap, 0);
    } else if (hm_wifi_secure(ap->security) < 0) { /* WEP, Enterprise: not here */
        toast(pw_tr(ap->security == HM_WIFI_SEC_WEP ? PW_S_WIFI_WEP_XMB : PW_S_WIFI_UNSUP), now);
        g_s.snd = HM_SND_ERROR;
    } else {
        g_kb_ap = *ap;
        stbsp_snprintf(title, sizeof(title), pw_tr(PW_S_WIFI_PASSWORD), ap->ssid);
        hm_kb_open(&g_kb, title, "", HM_WIFI_KEY - 1, 0);
        g_s.kb = 1;
        g_s.kb_for = KB_WIFI;
    }
}

/* ---- the battery's calibration: the end of a run, told once ---- */

static void cal_news(unsigned int now)
{
    static char text[64];
    int news = hm_hw_cal_news();
    hm_battcal c;

    if (news == HM_CAL_NEWS_DONE && hm_hw_cal_result(&c) == 0) {
        stbsp_snprintf(text, sizeof(text), pw_tr(PW_S_BAT_CAL_DONE), c.est_full_mah);
        toast(text, now);
    } else if (news == HM_CAL_NEWS_SHORT) {
        toast(pw_tr(PW_S_BAT_CAL_SHORT), now);
    }
}

/* ---- the battery's calibration screen (BAT-REQ-011): its words and numbers, by the second ---- */

static hm_calview g_calv;
static unsigned char g_cal_pct[HM_CAL_PTS];

static void dur(char *out, int cap, unsigned int s)
{
    unsigned int m = s / 60;

    if (m >= 60)
        stbsp_snprintf(out, cap, pw_tr(PW_S_DUR_HM), (int)(m / 60), (int)(m % 60));
    else
        stbsp_snprintf(out, cap, pw_tr(PW_S_DUR_M), (int)m);
}

static void cal_texts(void)
{
    static char big[16], st[5][48], left[16], right[32];
    hm_calview *c = &g_calv;
    hm_battery b;
    hm_battcal live, res;
    int have = hm_hw_cal_result(&res) == 0, n = 0, npts;

    hm_hw_battery(&b);
    npts = hm_hw_cal_live(&live, g_cal_pct, HM_CAL_PTS);
    memset(c, 0, sizeof(*c));
    c->title = pw_tr(PW_S_CAL_TITLE);
    c->steps[0] = pw_tr(PW_S_CAL_S1);
    c->steps[1] = pw_tr(PW_S_CAL_S2);
    c->steps[2] = pw_tr(PW_S_CAL_S3);
    c->steps[3] = pw_tr(PW_S_CAL_S4);
    c->step = b.cal == HM_CAL_CHARGE   ? 1
              : b.cal == HM_CAL_UNPLUG ? 2
              : b.cal == HM_CAL_RUN    ? 3
              : have                   ? 4
                                       : 0;
    c->what = pw_tr(c->step == 1   ? PW_S_CAL_DO_CHARGE
                    : c->step == 2 ? PW_S_CAL_DO_UNPLUG
                    : c->step == 3 ? PW_S_CAL_DO_RUN
                    : c->step == 4 ? PW_S_CAL_DO_DONE
                                   : PW_S_CAL_HOW);
    if (b.pct >= 0) {
        stbsp_snprintf(big, sizeof(big), "%d%%", b.pct);
        c->big = big;
        c->big_sub = pw_tr(b.charging ? PW_S_BAT_CHARGING
                           : b.online ? PW_S_BAT_ON_POWER
                                      : PW_S_BAT_ON_BATTERY);
    }
#define STAT(l, ...)                                                                               \
    do {                                                                                           \
        if (n < 4) {                                                                               \
            stbsp_snprintf(st[n], sizeof(st[n]), __VA_ARGS__);                                     \
            c->label[n] = pw_tr(l);                                                                \
            c->value[n] = st[n];                                                                   \
            n++;                                                                                   \
        }                                                                                          \
    } while (0)
    if (c->step == 3 && npts >= 0) { /* measuring: what it drew so far */
        char d[24];

        dur(d, sizeof(d), live.seconds);
        STAT(PW_S_CAL_ELAPSED, "%s", d);
        STAT(PW_S_CAL_USED, "%d mAh", live.mah);
        STAT(PW_S_CAL_AVERAGE, "%d mA", live.avg_ma);
        if (live.est_full_mah > 0)
            STAT(PW_S_CAL_PROJECTED, "~%d mAh", live.est_full_mah);
        else
            STAT(PW_S_CAL_PROJECTED, "%s", pw_tr(PW_S_CAL_LATER));
    } else if (c->step == 4) { /* the last one measured */
        char d[24];

        dur(d, sizeof(d), res.seconds);
        if (res.full_mah > 0)
            STAT(PW_S_BAT_MEASURED, "%d mAh  %d%%", res.est_full_mah,
                 res.est_full_mah * 100 / res.full_mah);
        else
            STAT(PW_S_BAT_MEASURED, "%d mAh", res.est_full_mah);
        STAT(PW_S_BAT_LASTED, "%s", d);
        STAT(PW_S_CAL_AVERAGE, "%d mA", res.avg_ma);
    }
    if (c->step != 3 && b.ma != PW_BATT_UNKNOWN && b.pct >= 0)
        STAT(PW_S_BAT_CURRENT, "%+d mA", b.ma);
    if (c->step == 3 && b.ma != PW_BATT_UNKNOWN)
        STAT(PW_S_BAT_CURRENT, "%d mA", b.ma);
    if (b.mv > 0)
        STAT(PW_S_BAT_VOLTAGE, "%d.%02d V", b.mv / 1000, b.mv % 1000 / 10);
    if (b.rcap > 0 && b.full > 0)
        STAT(PW_S_BAT_CAPACITY, "%d/%d", b.rcap, b.full);
    if (b.cycles >= 0)
        STAT(PW_S_BAT_CYCLES, "%d", b.cycles);
#undef STAT
    c->nstats = n;
    if (npts >= 2) {
        c->pct = g_cal_pct;
        c->npct = npts;
        dur(right, sizeof(right), live.seconds);
        stbsp_snprintf(left, sizeof(left), "%d%%", live.pct_from);
        c->chart_left = left;
        c->chart_right = right;
    } else {
        c->chart_empty = pw_tr(PW_S_CAL_NO_CHART);
    }
}

/* ---- the volume and brightness indicator, the brightness boost (HOM-REQ-034, ADR-045): read
   every 100 ms ---- */

#define AV_MS 100

static pw_osd g_osd;
static pw_bright g_bright;
static pw_steps g_steps;

static void av_frame(unsigned int now, unsigned int buttons)
{
    static unsigned int t_av, t_screen;
    static int level = -1, told, steps_set;
    static char text[32];
    const pw_perf_cfg *c = hs_perf_cfg();
    int kind, set, tap = 0, n = c->steps ? c->nlevels : 4;

    if (g_s.screen_off ||
        (c->osd == PW_OSD_OFF && c->boost == 0 && !c->steps && g_bright.set < 0)) {
        g_v.osd = 0;
        return;
    }
    if (buttons & PSP_CTRL_SCREEN) /* (a change of the owner's: no fight) */
        t_screen = now;
    if (c->steps) /* a tap: Pocketwatch's next step (ADR-047) */
        tap = pw_steps_button(&g_steps, (buttons & PSP_CTRL_SCREEN) != 0, c->nlevels, now);
    if (tap || now - t_av >= AV_MS) {
        hm_hw_av av;

        t_av = now;
        hm_hw_av_read(&av);
        level = av.level;
        if (c->steps && !steps_set && av.level > 0) {
            g_steps.cur = pw_steps_nearest(c, av.level);
            steps_set = 1;
        }
        if ((set = pw_bright_step(&g_bright, av.level, c->boost,
                                  c->steps ? c->levels[g_steps.cur % c->nlevels] : 0,
                                  now - t_screen < 1000, now)) > 0 &&
            hm_hw_set_level(set) == 0)
            level = set;
        if (g_bright.fights >= PW_BRIGHT_FIGHTS && !told) {
            hm_hw_log("home brightness: another plugin sets its level back: the boost gave up\n");
            told = 1;
        }
        if (c->osd != PW_OSD_OFF)
            pw_osd_step(&g_osd, av.vol, av.mute, c->steps ? g_steps.cur : av.bright, now);
    }
    kind = c->osd != PW_OSD_OFF ? g_osd.kind : PW_OSD_NONE; /* (its words stay as it fades) */
    g_v.osd = !kind                       ? 0
              : now - g_osd.t < PW_OSD_MS ? 256
                                          : 256 - pw_ramp(now, g_osd.t + PW_OSD_MS, 300);
    if (kind == PW_OSD_VOLUME) {
        g_v.osd_glyph = HM_ICON_SPEAKER;
        g_v.osd_level =
            g_osd.mute == 1 || g_osd.vol < 0 ? 0 : (g_osd.vol > 30 ? 30 : g_osd.vol) * 256 / 30;
        if (g_osd.mute == 1)
            stbsp_snprintf(text, sizeof(text), "%s", pw_tr(PW_S_OSD_MUTED));
        else
            stbsp_snprintf(text, sizeof(text), "%d", g_osd.vol);
        g_v.osd_text = text;
    } else if (kind == PW_OSD_BRIGHT) {
        g_v.osd_glyph = HM_ICON_SUN;
        g_v.osd_level = level < 0 ? 0 : (level > 100 ? 100 : level) * 256 / 100;
        stbsp_snprintf(text, sizeof(text), "%d/%d   %d%%", g_osd.bright + 1, n, level);
        g_v.osd_text = text;
    }
}

/* ---- the performance overlay over Home (ADR-043): its numbers once a second ---- */

static struct {
    unsigned int t0, frames, cpu_us, ge_us, worst_ms, last;
} g_perf;
static char g_perf_fps[8], g_perf_val[6][32];

static void perf_frame(unsigned int now, unsigned int cpu_us, unsigned int ge_us)
{
    if (g_perf.frames && now - g_perf.last > g_perf.worst_ms)
        g_perf.worst_ms = now - g_perf.last;
    g_perf.last = now;
    g_perf.frames++;
    g_perf.cpu_us += cpu_us;
    g_perf.ge_us += ge_us;
    if (!hm_cfg_get()->perf) {
        g_v.perf_fps = 0;
        return;
    }
    if (now - g_perf.t0 >= 1000) {
        unsigned int ms = now - g_perf.t0, n = g_perf.frames ? g_perf.frames : 1;
        unsigned int free_kb = (unsigned int)sceKernelTotalFreeMemSize() >> 10;
        hm_battery b;
        int k = 0;

        hm_hw_battery(&b);
        stbsp_snprintf(g_perf_fps, sizeof(g_perf_fps), "%u", g_perf.frames * 1000 / ms);
#define VAL(label, ...)                                                                            \
    do {                                                                                           \
        stbsp_snprintf(g_perf_val[k], sizeof(g_perf_val[k]), __VA_ARGS__);                         \
        g_v.perf_label[k] = pw_tr(label);                                                          \
        g_v.perf_value[k] = g_perf_val[k];                                                         \
        k++;                                                                                       \
    } while (0)
        VAL(PW_S_PERF_FRAME, "%u.%u ms", g_perf.cpu_us / n / 1000, g_perf.cpu_us / n % 1000 / 100);
        VAL(PW_S_PERF_SLOWEST, "%u ms", g_perf.worst_ms);
        VAL(PW_S_PERF_CPU, "%d MHz", scePowerGetCpuClockFrequencyInt());
        VAL(PW_S_PERF_RAM, "%u.%u MB", free_kb / 1024, free_kb % 1024 * 10 / 1024);
        if (b.pct >= 0 && b.ma != PW_BATT_UNKNOWN) /* (what the system does not give: left out) */
            VAL(PW_S_BATTERY, "%d%%   %d mA", b.pct, b.ma);
        else if (b.pct >= 0)
            VAL(PW_S_BATTERY, "%d%%", b.pct);
        if (b.mv > 0 && b.temp > 0 && b.temp < 80)
            VAL(PW_S_BAT_VOLTAGE,
                "%d.%02d V   %d \xC2\xB0"
                "C",
                b.mv / 1000, b.mv % 1000 / 10, b.temp);
#undef VAL
        g_v.perf_n = k;
        g_v.perf_fps = g_perf_fps;
        memset(&g_perf, 0, sizeof(g_perf));
        g_perf.t0 = g_perf.last = now;
    }
}

/* ---- Home's keyboard (HOM-REQ-030) ---- */

static void kb_close(int save)
{
    if (g_s.kb_for == KB_WIFI) {
        if (save)
            net_start(HM_JOB_JOIN, 0, &g_kb_ap, g_kb.text);
        memset(g_kb.text, 0, sizeof(g_kb.text)); /* (the password is not kept) */
        g_s.kb_saved = 1;
    } else if (save && hm_kb_changed(&g_kb) && hs_set_nickname(g_kb.text) == 0) {
        g_s.kb_saved = 1;
    }
    g_s.kb = 0;
}

/* Done on the keyboard: what it typed must do (a nickname not empty, a WPA key's length). */
static int kb_ok(void)
{
    int hex;

    return g_s.kb_for == KB_WIFI ? hm_wifi_key_ok(g_kb.text, &hex) : g_kb.text[0] != 0;
}

/* The PSP keyboard's own buttons: Square erases, Triangle a space, L / R the caret, SELECT the
   next page, START done; Back with changes asks first (the owner wanted that question in
   Pocketwatch's look too). */
static void kb_keys(unsigned int ev)
{
    hm_kb *k = &g_kb;

    g_s.snd = HM_SND_MOVE;
    if (k->ask) {
        if (ev & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT))
            k->ask_sel = (k->ask_sel + (ev & PSP_CTRL_LEFT ? 2 : 1)) % 3;
        else if ((ev & g_enter) && k->ask_sel == HM_KB_ASK_EDIT)
            k->ask = 0;
        else if ((ev & g_enter) && k->ask_sel == HM_KB_ASK_SAVE && !kb_ok())
            k->ask = 0, toast(pw_tr(PW_S_WIFI_KEY_BAD), hm_hw_now_ms()), g_s.snd = HM_SND_ERROR;
        else if (ev & g_enter)
            kb_close(k->ask_sel == HM_KB_ASK_SAVE), g_s.snd = HM_SND_OPEN;
        else if (ev & (g_back | PSP_CTRL_HOME))
            k->ask = 0, g_s.snd = HM_SND_BACK;
        return;
    }
    if (ev & (g_back | PSP_CTRL_HOME)) {
        if (hm_kb_changed(k)) {
            k->ask = 1;
            k->ask_sel = HM_KB_ASK_SAVE;
            g_s.snd = HM_SND_OPEN;
        } else {
            kb_close(0);
            g_s.snd = HM_SND_BACK;
        }
    } else if ((ev & PSP_CTRL_START) ||
               ((ev & g_enter) && hm_kb_special(k, k->row, k->col) == HM_KB_DONE)) {
        if (kb_ok()) /* (the system keeps no empty nickname; WPA: 8-63 characters) */
            kb_close(1), g_s.snd = HM_SND_OPEN;
        else if (g_s.kb_for == KB_WIFI)
            toast(pw_tr(PW_S_WIFI_KEY_BAD), hm_hw_now_ms()), g_s.snd = HM_SND_ERROR;
        else
            g_s.snd = HM_SND_ERROR;
    } else if (ev & g_enter) {
        hm_kb_press(k);
    } else if (ev & PSP_CTRL_SQUARE) {
        hm_kb_erase(k);
    } else if (ev & PSP_CTRL_TRIANGLE) {
        hm_kb_type(k, " ");
    } else if (ev & PSP_CTRL_SELECT) {
        hm_kb_next_page(k);
    } else if (ev & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) {
        hm_kb_caret(k, ev & PSP_CTRL_LTRIGGER ? -1 : 1);
    } else if (ev & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) {
        hm_kb_move(k, ev & PSP_CTRL_LEFT ? -1 : 1, 0);
    } else if (ev & (PSP_CTRL_UP | PSP_CTRL_DOWN)) {
        hm_kb_move(k, 0, ev & PSP_CTRL_UP ? -1 : 1);
    } else {
        g_s.snd = -1;
    }
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
static void after_setting(int r, int was_sort, int was_style);
static int enter_row(int r, unsigned int now, int *nickname, int *browser, int *start_usb);

/* The two-pane settings (Modern, ADR-050): the sidebar is the categories (the page's rows), the
   right pane the chosen category's options (p->opt). D-pad up / down move within the focused pane;
   Enter / right enters the options (or opens Wi-Fi / Calibration); left / Back returns to the
   sidebar; left / right change a choice. */
static void settings_keys(unsigned int ev, unsigned int now, int *nickname, int *browser,
                          int *start_usb)
{
    hm_page *p = &g_v.page[0];
    int was = pw_config_get()->sort, style = style_now();

    if (!p->opt_focus) {
        if (ev & (PSP_CTRL_UP | PSP_CTRL_DOWN)) {
            p->row_sel = hm_row_next(p->rows, p->nrows, p->row_sel, ev & PSP_CTRL_UP ? -1 : 1);
            p->opt_sel = p->opt_pos = 0;
            build_all();
            g_s.snd = HM_SND_MOVE;
        } else if (ev & (g_enter | PSP_CTRL_RIGHT)) {
            if (p->opt_n > 0) {
                p->opt_focus = 1;
                p->opt_sel = hm_row_next(p->opt, p->opt_n, -1, 1);
                g_s.snd = HM_SND_OPEN;
            } else {
                enter_row(p->row_sel, now, nickname, browser, start_usb); /* Wi-Fi / Calibration */
            }
        }
    } else {
        hm_page_id pid = {P_SET_CAT, 0};
        hm_row_act a = hm_page_act(pid, p->opt_sel);

        if (ev & (PSP_CTRL_UP | PSP_CTRL_DOWN)) {
            p->opt_sel = hm_row_next(p->opt, p->opt_n, p->opt_sel, ev & PSP_CTRL_UP ? -1 : 1);
            g_s.snd = HM_SND_MOVE;
        } else if ((ev & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) && a.act == A_SETTING) {
            after_setting(hs_step(a.arg, ev & PSP_CTRL_LEFT ? -1 : 1), was, style);
            if (g_s.snd < 0)
                g_s.snd = HM_SND_MOVE;
        } else if (ev & (PSP_CTRL_LEFT | g_back)) {
            p->opt_focus = 0;
            g_s.snd = HM_SND_BACK;
        } else if (ev & g_enter) {
            if (a.act == A_SETTING) {
                after_setting(hs_step(a.arg, 1), was, style);
                if (g_s.snd < 0)
                    g_s.snd = HM_SND_MOVE;
            } else if (a.act == A_NICKNAME) {
                *nickname = 1;
            } else if (a.act == A_NET_TIME) {
                net_start(HM_JOB_TIME, 0, 0, 0);
                g_s.snd = HM_SND_OPEN;
            }
        }
    }
}

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
    if (r == HS_NEXT_GAME) /* the game overlay's module: ARK-4 loads it at a game's start */
        toast(pw_tr(PW_S_HS_PERF_NEXT), hm_hw_now_ms());
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
        if (a.arg == P_GAME || a.arg == P_STATES || a.arg == P_SAVES || a.arg == P_INFO ||
            a.arg == P_ALBUM || a.arg == P_WIFI_NET)
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
        if (!g_env.net_job) /* (the network is a job's meanwhile) */
            *browser = 1;
        return 1;
    case A_NET_TIME:
        net_start(HM_JOB_TIME, 0, 0, 0);
        g_s.snd = HM_SND_OPEN;
        return 1;
    case A_WIFI_SCAN:
        net_start(HM_JOB_SCAN, 0, 0, 0);
        g_s.snd = HM_SND_OPEN;
        return 1;
    case A_WIFI_JOIN:
        if (!g_env.net_job)
            wifi_join(a.arg, now);
        if (g_s.snd < 0)
            g_s.snd = HM_SND_OPEN;
        return 1;
    case A_WIFI_TEST:
        net_start(HM_JOB_TEST, a.arg, 0, 0);
        g_s.snd = HM_SND_OPEN;
        return 1;
    case A_WIFI_DELETE:
        g_s.confirm = CONFIRM_WIFI;
        g_s.confirm_arg = a.arg;
        g_s.snd = HM_SND_OPEN;
        return 1;
    case A_CAL: {
        hm_battery b;

        hm_hw_battery(&b);
        if (b.cal == HM_CAL_OFF) {
            hm_hw_cal(1);
        } else { /* hours of measuring: asked first */
            g_s.confirm = CONFIRM_CAL;
            g_s.confirm_arg = 0;
        }
        g_s.snd = HM_SND_OPEN;
        return 1;
    }
    case A_EXIT:
        g_s.leave = 1;
        return 1;
    case A_SLEEP:
        hs_save_pending(1);
        scePowerRequestSuspend();
        return 1;
    case A_OFF:
        hs_save_pending(1);
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

static void music_mode_toggle(int shuffle);

static void now_opened(void)
{
    g_s.mu_flash = -1;
    g_s.mu_keys = ~0u; /* a key still down from the page before is not a press */
}

static void now_flash(int btn, unsigned int now)
{
    g_s.mu_flash = btn;
    g_s.mu_flash_t = now;
}

/* Now Playing (ADR-057): each key does one thing, whatever was pressed before - left / right
   seek (a press SEEK_TAP, held a step every SEEK_EVERY), up / down the track before / after (as
   up / down move in the track list), Enter plays or pauses, Square and Triangle shuffle and repeat
   (page_keys). Owner 2026-10-29: with ADR-054's focus (up / down to the bar or the buttons, left /
   right seeking on one, choosing on the other) "up and right do the same, down and left too".
   Every button shows its key; the one a key works flashes. Only presses count for the d-pad: its
   repeats every 100 ms sought 10 s each and would skip a track each. */
static unsigned int now_keys(unsigned int ev, unsigned int buttons, unsigned int now)
{
    unsigned int lr = PSP_CTRL_LEFT | PSP_CTRL_RIGHT, ud = PSP_CTRL_UP | PSP_CTRL_DOWN,
                 press = ev & ~g_s.mu_keys;

    g_s.mu_keys = buttons;
    if (press & lr) {
        g_s.hold_dir = press & PSP_CTRL_LEFT ? -1 : 1;
        g_s.hold_t0 = g_s.hold_last = now;
        hm_music_seek(g_s.hold_dir * SEEK_TAP);
    } else if ((press & ud) && hm_music_current() >= 0) {
        int up = (press & PSP_CTRL_UP) != 0;

        hm_music_skip(up ? -1 : 1);
        now_flash(up ? HM_MU_PREV : HM_MU_NEXT, now);
        g_s.snd = HM_SND_MOVE;
    } else if (ev & g_enter) {
        if (hm_music_current() >= 0)
            hm_music_toggle();
        else if (hm_music_count() > 0) /* nothing yet: the first track (the hint says Play) */
            hm_music_play(0);
        now_flash(HM_MU_PLAY, now);
        g_s.snd = HM_SND_OPEN;
    }
    if (g_s.hold_dir && (buttons & (g_s.hold_dir < 0 ? PSP_CTRL_LEFT : PSP_CTRL_RIGHT))) {
        if (now - g_s.hold_t0 >= HOLD_MS && now - g_s.hold_last >= SEEK_EVERY) {
            hm_music_seek(g_s.hold_dir * SEEK_STEP);
            g_s.hold_last = now;
        }
    } else {
        g_s.hold_dir = 0;
    }
    g_v.mu_seek = g_s.hold_dir;
    return ev & ~(lr | ud | g_enter);
}

/* What the key just pressed did, under the buttons while its ring fades ("Shuffle: On",
   "Repeat: All", "Next Track"), and the keys in the hints. */
static void now_texts(unsigned int now)
{
    static char text[48];
    const hm_cfg *c = hm_cfg_get();
    int playing = hm_music_current() >= 0 && !hm_music_paused(), el = (int)(now - g_s.mu_flash_t),
        flash = el < FLASH_MS ? g_s.mu_flash : -1;

    switch (flash) {
    case HM_MU_SHUFFLE:
        stbsp_snprintf(text, sizeof(text), "%s: %s", pw_tr(PW_S_HS_SHUFFLE),
                       pw_tr(c->shuffle ? PW_S_ON : PW_S_OFF));
        break;
    case HM_MU_REPEAT:
        stbsp_snprintf(text, sizeof(text), "%s: %s", pw_tr(PW_S_HS_REPEAT),
                       pw_tr(c->repeat == 2   ? PW_S_HS_REPEAT_ONE
                             : c->repeat == 1 ? PW_S_HS_REPEAT_ALL
                                              : PW_S_OFF));
        break;
    case HM_MU_PREV:
    case HM_MU_NEXT:
        stbsp_snprintf(text, sizeof(text), "%s",
                       pw_tr(flash == HM_MU_PREV ? PW_S_HM_PREV_TRACK : PW_S_HM_NEXT_TRACK));
        break;
    case HM_MU_PLAY:
        stbsp_snprintf(text, sizeof(text), "%s", pw_tr(playing ? PW_S_HM_PAUSE : PW_S_HM_PLAY));
        break;
    default:
        text[0] = 0;
    }
    if (hm_music_current() < 0) { /* no player drawn: only the way to start one */
        g_v.hint_enter = hm_music_count() > 0 ? pw_tr(PW_S_HM_PLAY) : 0;
        return;
    }
    g_v.mu_flash = flash;
    g_v.mu_flash_a = el < FLASH_MS / 2 ? 256 : (FLASH_MS - el) * 512 / FLASH_MS;
    g_v.mu_flash_text = text[0] ? text : 0;
    g_v.hint_enter = pw_tr(playing ? PW_S_HM_PAUSE : PW_S_HM_PLAY);
    g_v.hint_lr = pw_tr(PW_S_HM_SEEK);
    g_v.hint_ud = pw_tr(PW_S_HM_TRACK);
}

static void music_mode_toggle(int shuffle)
{
    hm_cfg *c = hm_cfg_get();

    if (shuffle)
        c->shuffle = !c->shuffle;
    else
        c->repeat = (c->repeat + 1) % 3;
    hs_home_changed(); /* (written once the presses stop) */
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
    if (id == P_SETTINGS && modern && depth == 0) { /* the two-pane settings (ADR-050) */
        settings_keys(ev, now, nickname, browser, start_usb);
        return;
    }
    if ((ev & PSP_CTRL_LEFT) && cl && depth > 0 && id != P_NOW) { /* the XMB: left goes back */
        pop();
        g_s.snd = HM_SND_BACK;
        return;
    }
    if ((id == P_MUSIC || id == P_NOW) && (ev & PSP_CTRL_START) && hm_music_count()) {
        hm_music_toggle(); /* START: pause or play on both music pages */
        if (id == P_NOW)
            now_flash(HM_MU_PLAY, now);
        g_s.snd = HM_SND_OPEN;
        return;
    }
    if (id == P_NOW)
        ev = now_keys(ev, buttons, now);
    if ((ev & g_back) && depth > 0) {
        pop();
        g_s.snd = HM_SND_BACK;
        return;
    }
    switch (id) {
    case P_GAMES:
        if (p->is_dash) { /* the launcher: the Resume hero (focus 0), five section tiles (1..5) */
            static const int stab[5] = {HM_TAB_GAMES, HM_TAB_MUSIC, HM_TAB_PHOTOS, HM_TAB_SETTINGS,
                                        HM_TAB_SYSTEM};
            int f = p->opt_sel;

            if (f == 0) {
                if (ev & (PSP_CTRL_DOWN | PSP_CTRL_RIGHT)) {
                    p->opt_sel = 1;
                    g_s.snd = HM_SND_MOVE;
                } else if ((ev & g_enter) && p->count) {
                    launch(p->hero, 0, now); /* play the most recently played game */
                } else if ((ev & PSP_CTRL_TRIANGLE) && p->count) {
                    push(P_GAME, p->hero);
                    g_s.snd = HM_SND_OPEN;
                }
            } else if (ev & PSP_CTRL_UP) {
                p->opt_sel = 0;
                g_s.snd = HM_SND_MOVE;
            } else if (ev & PSP_CTRL_LEFT) {
                p->opt_sel = f > 1 ? f - 1 : 0;
                g_s.snd = HM_SND_MOVE;
            } else if (ev & PSP_CTRL_RIGHT) {
                p->opt_sel = f < 5 ? f + 1 : 5;
                g_s.snd = HM_SND_MOVE;
            } else if (ev & g_enter) {
                if (stab[f - 1] == HM_TAB_GAMES)
                    push(P_GAMES, 0); /* the full library (depth 1: the plain carousel) */
                else
                    open_tab(stab[f - 1]);
                g_s.snd = HM_SND_TAB;
            }
            break;
        }
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
            push(P_ALBUM, hm_pages_album_at(p->sel));
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
    case P_NOW: /* (the d-pad and Enter: now_keys) the buttons at the two ends */
        if (ev & (PSP_CTRL_SQUARE | PSP_CTRL_TRIANGLE)) {
            music_mode_toggle((ev & PSP_CTRL_SQUARE) != 0);
            now_flash(ev & PSP_CTRL_SQUARE ? HM_MU_SHUFFLE : HM_MU_REPEAT, now);
        }
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
    g_v.hint_lr = g_v.hint_ud = 0;
    g_v.mu_flash = -1; /* (the player's: now_texts) */
    g_v.mu_flash_text = 0;
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
    case P_CAL: {
        hm_battery b;

        hm_hw_battery(&b);
        g_v.hint_enter = pw_tr(b.cal == HM_CAL_OFF ? PW_S_BAT_CAL_START : PW_S_CAL_STOP);
        break;
    }
    case P_NOW:
        now_texts(hm_hw_now_ms());
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
        else if (a.act == A_WIFI_SCAN || a.act == A_WIFI_TEST || a.act == A_WIFI_JOIN)
            g_v.hint_enter = g_env.net_job ? 0 : pw_tr(PW_S_OK);
        else if (a.act == A_WIFI_DELETE)
            g_v.hint_enter = pw_tr(PW_S_DELETE);
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
    } else if (g_s.confirm == CONFIRM_WIFI) {
        const hm_wifi_conn *c = hm_pages_wifi_conn(g_s.confirm_arg);

        stbsp_snprintf(text, sizeof(text), pw_tr(PW_S_WIFI_DELETE_Q), c ? c->ssid : "");
        g_v.sheet_text = text;
        g_v.sheet_glyph = HM_ICON_GLOBE;
    } else if (g_s.confirm == CONFIRM_CAL) {
        g_v.sheet_glyph = HM_ICON_BATTERY;
        g_v.sheet_title = pw_tr(PW_S_CAL_TITLE);
        g_v.sheet_text = pw_tr(PW_S_CAL_STOP_Q);
        g_v.sheet_again = pw_tr(PW_S_CAL_STOP);
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
    } else if (g_s.confirm == CONFIRM_CAL) {
        hm_hw_cal(0);
        g_s.confirm = CONFIRM_NONE;
        g_s.snd = HM_SND_BACK;
        build_all();
        return;
    } else if (g_s.confirm == CONFIRM_WIFI) {
        ok = hm_net_delete(g_s.confirm_arg) < 0 ? -1 : 0;
        hm_pages_wifi_read();
        if (id == P_WIFI_NET)
            pop();
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
    } else if (g_v.panel_sel == PANEL_BRIGHT && (ev & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT))) {
        int bl = hm_bright_get(); /* the brightness slider: left / right, the panel stays open */

        if (bl >= 0 && hm_bright_set(bl + (ev & PSP_CTRL_RIGHT ? 1 : -1)) == 0) {
            int sel = g_v.panel_sel;

            panel_home(&g_v); /* refresh the slider and the level text from the new brightness */
            g_v.panel_sel = sel;
            g_s.snd = HM_SND_MOVE;
        }
    } else if (ev & (g_back | PSP_CTRL_HOME)) {
        g_s.panel_open = 0;
        g_s.snd = HM_SND_BACK;
    } else if ((ev & g_enter) && g_v.panel_sel != PANEL_BRIGHT) {
        g_s.panel_open = 0;
        hs_save_pending(1); /* (the settings' files before USB, sleep, a restart, leaving) */
        if (g_v.panel_sel == PANEL_USB)
            *start_usb = 1;
        else if (g_v.panel_sel == PANEL_XMB)
            g_s.leave = 1;
        else if (g_v.panel_sel == PANEL_SLEEP)
            scePowerRequestSuspend();
        else if (g_v.panel_sel == PANEL_RESTART) { /* relaunch Home (same path as after USB) */
            hm_sound_end();
            hm_launch_eboot(HOME_SELF, 0);
            sceKernelExitGame();
        } else if (g_v.panel_sel == PANEL_OFF)
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
/* After USB, Home starts again before it writes anything (ADR-048): the FAT driver still holds
   the card's tables as they were before the computer changed them, and a user program may not drop
   them (0x800200D1, 2026-10-21) - a write from them put seplugins/pocketwatch back and lost what
   the PC wrote, twice, the second time the PRXs themselves. A start reboots the kernel and its FAT
   driver with it; until then the card stays lent (nothing of Home's writes). */
static void usb_end(void)
{
    hm_usb_stop();
    hm_sound_end();
    hm_launch_eboot(HOME_SELF, 0);
    sceKernelExitGame(); /* (the start failed: the XMB - a reboot too) */
}

static void usb_begin(unsigned int now)
{
    hs_save_pending(1); /* the settings written while the card is still ours */
    hm_music_stop();    /* the loader, the tags and the music leave the card first */
    hm_music_pause_tags(1);
    hm_library_pause(1);
    g_s.usb = 1;
    g_s.usb_state = hm_usb_start(now);
    usb_log(); /* the drivers: before the connection, the card is still ours */
    hm_hw_card_lent(1);
    if (g_s.usb_state != HM_USB_FAILED)
        g_s.usb_state = hm_usb_poll(now);
    g_s.t_usb = now;
}

static void launch_now(stats *st, unsigned int now)
{
    hm_game *gm = hm_library_game(g_s.launching);

    stats_write(st, now, "before a game");
    hs_save_pending(1);
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
    unsigned int t_prev, t_rows = 0, t_status = 0, t_boot = 0;
    const unsigned int *shown = 0; /* the background drawn last */
    int up = 0, clock_was;
    char clock[24] = "";

    /* first: before anything writes a file, whose time would be the lost clock's */
    clock_was = hm_hw_clock_check();
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
    g_env.net = &g_net;
    pw_bright_init(&g_bright);
    /* PLUGINS.TXT and perf.txt now, behind the start: read by the first frame's indicator they
       made it 90-120 ms long at every start (home.log 2026-10-20..27, "p0 keys 92") */
    (void)hs_perf_cfg();
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
    t_boot = g_s.t_open; /* the start-up animation's reference (ADR-052) */
    memset(&st, 0, sizeof(st));
    st.t0 = st.last = g_s.t_open;
    g_perf.t0 = g_s.t_open;
    /* a lost clock: what was done about it, and the internet's time when Wi-Fi is on */
    if (clock_was != HM_CLOCK_OK) {
        toast(pw_tr(clock_was == HM_CLOCK_RESTORED ? PW_S_CLK_RESTORED : PW_S_CLK_LOST),
              g_s.t_open);
        if (hm_net_switch() == 1)
            net_start(HM_JOB_TIME, 0, 0, 0);
    }

    while (!hm_hw_exit_requested() && !g_s.leave) {
        unsigned int now = hm_hw_now_ms(), t_cpu = sceKernelGetSystemTimeLow(), t_warm,
                     buttons = hm_hw_buttons(),
                     ev = pw_keys_update(&keys, buttons, REPEAT, now * 1000);
        const unsigned int *want;
        unsigned int ge;
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
            hs_save_pending(1);
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
        } else if (g_s.launching < 0 && g_s.kb) {
            kb_keys(ev);
        } else if (g_s.launching < 0 && g_s.panel_open) {
            panel_keys(ev, &start_usb);
        } else if (g_s.launching < 0 && (ev & PSP_CTRL_HOME)) {
            panel_home(&g_v);
            g_s.panel_open = 1;
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
        if (nickname) { /* Home's own keyboard, not the firmware's (HOM-REQ-030) */
            char was[HM_KB_BYTES];

            hs_nickname(was, sizeof(was));
            hm_kb_open(&g_kb, pw_tr(PW_S_HS_NICKNAME), was, 32, /* the XMB's nickname length */
                       strcmp(hm_texts_code(), "uk") == 0);
            g_s.kb = 1;
            g_s.kb_for = KB_NICKNAME;
        }
        if (g_s.kb_saved) {
            g_s.kb_saved = 0;
            t_rows = 0; /* the row shows the new name */
        }
        if (!g_s.usb)           /* (never while the card is the PC's) */
            hs_save_pending(0); /* the settings' files, once the changes stop */
        net_poll(now);          /* a network job's answer */
        av_frame(now, buttons); /* the volume and brightness indicator, the boost, the steps */
        cal_news(now);          /* a calibration run ended */
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
            g_v.free_mb = hm_library_free_mb(); /* cached count, cheap to read */
        }
        {
            unsigned int t0 = sceKernelGetSystemTimeLow(), us;
            int id = top_id();

            g_t_keys = t0 - t_cpu;
            if (t_rows == 0 || now - t_rows >= 1000) {
                build_all();
                t_rows = now;
                g_v.cal = 0; /* the calibration's screen, while one of the pages is it */
                for (int d = 0; d < HM_DEPTH && d <= g_v.depth + 1; d++)
                    if (g_stack[d].id == P_CAL && g_v.page[d].layout == HM_PAGE_CAL)
                        cal_texts(), g_v.cal = &g_calv;
            } else if (id == P_MUSIC || id == P_NOW || id == P_SAVES || id == P_ALBUM ||
                       id == P_ALBUMS || id == P_GAMES) {
                /* the time playing, the pictures arriving: the page shown only */
                hm_page_build(&g_v, g_v.depth, g_stack[g_v.depth], &g_env);
            }
            us = sceKernelGetSystemTimeLow() - t0;
            if (us > st.rows_us)
                st.rows_us = us;
            g_t_rows = us;
        }
        g_v.battery_pct = hm_cfg_get()->percent;
        page_hints();
        loader_focus();
        /* motion, by time */
        for (int d = 0; d < HM_DEPTH; d++) {
            hm_page *p = &g_v.page[d];
            int cols = g_v.style != HM_STYLE_MODERN ? 1
                       : g_stack[d].id == P_ALBUM   ? HM_PHOTO_COLS
                       : g_stack[d].id == P_ALBUMS  ? HM_ALBUM_COLS
                                                    : 1;

            p->selpos = pw_ease(p->selpos, p->sel * 256, dt, tau, 6);
            p->row_pos = pw_ease(p->row_pos, p->row_sel * 256, dt, tau, 6);
            p->grid_pos = pw_ease(p->grid_pos, p->sel / cols * 256, dt, tau, 6);
            p->opt_pos = pw_ease(p->opt_pos, p->opt_sel * 256, dt, tau, 6); /* settings two-pane */
            p->opt_focus_pos = pw_ease(p->opt_focus_pos, p->opt_focus * 256, dt, tau, 6);
        }
        g_v.depth_pos = pw_ease(g_v.depth_pos, g_v.depth * 256, dt, tau * 3 / 2, 6);
        g_v.tab_pos = pw_ease(g_v.tab_pos, g_v.tab * 256, dt, tau, 6);
        g_v.panel = pw_ease(g_v.panel, g_s.panel_open ? 256 : 0, dt, tau, 6);
        g_v.panel_pos = pw_ease(g_v.panel_pos, g_v.panel_sel * 256, dt, tau, 6);
        g_v.sheet = pw_ease(g_v.sheet, g_s.usb || g_s.confirm ? 256 : 0, dt, tau, 6);
        g_v.popup = pw_ease(g_v.popup, g_s.popup ? 256 : 0, dt, tau, 6);
        g_v.popup_pos = pw_ease(g_v.popup_pos, g_v.popup_sel * 256, dt, tau, 6);
        g_v.editor = pw_ease(g_v.editor, g_s.editor ? 256 : 0, dt, tau, 6);
        g_v.kb_t = pw_ease(g_v.kb_t, g_s.kb ? 256 : 0, dt, tau, 6);
        g_v.kb = g_s.kb || g_v.kb_t > 0 ? &g_kb : 0;
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
        { /* the start-up animation (ADR-052): the wordmark holds, then fades to the launcher;
             any button skips straight to the fade-out */
            unsigned int el = now - t_boot;

            if (buttons && el < INTRO_HOLD_MS)
                t_boot = now - INTRO_HOLD_MS, el = INTRO_HOLD_MS;
            g_v.intro = el >= INTRO_HOLD_MS + INTRO_FADE_MS ? 0
                        : el >= INTRO_HOLD_MS
                            ? 256 - (int)(el - INTRO_HOLD_MS) * 256 / INTRO_FADE_MS
                            : 256;
        }

        /* the frame: the list, then - while the GE draws it - what the next frames may need */
        g_t_draw = sceKernelGetSystemTimeLow();
        g_t_state = g_t_draw - t_cpu - g_t_keys - g_t_rows;
        g = hm_gu_begin();
        hm_draw(g, &g_ui, &g_v);
        hm_gu_finish();
        t_warm = sceKernelGetSystemTimeLow();
        g_t_draw = t_warm - g_t_draw;
        t_cpu = t_warm - t_cpu;
        hm_prefetch(g, &g_ui, &g_v);
        t_warm = sceKernelGetSystemTimeLow() - t_warm;
        g_t_warm = t_warm;
        ge = hm_gu_end();
        stats_frame(&st, now, t_cpu, t_warm, ge, top_id());
        perf_frame(now, t_cpu, ge);
        if (!up) { /* the first screen is up: tell the XMB plugin's guard */
            hm_hw_remove(PENDING);
            up = 1;
        }
        if (!g_s.usb && now - st.t0 >= STATS_MS)
            stats_write(&st, now, "running");
        if (g_s.launching >= 0 && now - g_s.t_fade >= FADE_MS)
            launch_now(&st, now);
    }
    if (g_s.usb) { /* (leaving in the middle of USB: nothing written, ADR-048) */
        hm_usb_stop();
        sceKernelExitGame();
        return 0;
    }
    if (g_s.screen_off) /* (never left dark) */
        hm_hw_backlight(1);
    hs_save_pending(1);
    stats_write(&st, hm_hw_now_ms(), "leaving");
    hm_sound_end();
    hm_hw_touch(XMB_ONCE); /* leaving Home means the XMB, even when Home is the start screen */
    sceKernelExitGame();
    return 0;
}
