// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>
#include <string.h>

#include "complications/states/engine.h"
#include "core/config.h"
#include "core/log.h"
#include "gfx/assets.h"
#include "stb_sprintf.h"
#include "ui/anim.h"
#include "ui/dialog.h"
#include "ui/fb.h"
#include "ui/i18n.h"
#include "ui/icons.h"
#include "ui/lang.h"
#include "ui/langnames.h"
#include "complications/complication.h"
#include "ui/views.h"

enum { SET_STYLE, SET_FAST, SET_STATUS, SET_LANGUAGE, SET_COUNT };

static const int g_style_names[PW_STYLE_COUNT] = {PW_S_STYLE_XMB, PW_S_STYLE_MODERN,
                                                  PW_S_STYLE_MINIMAL};

#define LANGS pw_lang_name_count

static void config_path(char *out, unsigned int cap)
{
    char dir[200];

    if (pw_log_dir(dir, sizeof(dir)) < 0)
        strcpy(dir, "ms0:/seplugins/pocketwatch/");
    stbsp_snprintf(out, (int)cap, "%sconfig.txt", dir);
}

static void settings_init(void)
{
    char path[256];

    config_path(path, sizeof(path));
    pw_config_load(path);
    pw_engine_set_fast(pw_config_get()->fast);
}

static int language_index(void)
{
    return pw_lang_name_index(pw_lang_chosen());
}

static const char *language_name(int i)
{
    return i == 0 ? pw_tr(PW_S_SYSTEM_LANGUAGE) : pw_lang_names[i].name;
}

typedef struct {
    pw_hub_cat cat;
    pw_setting_row rows[SET_COUNT];
} screen;

static void build(screen *s)
{
    const pw_config *cfg = pw_config_get();

    s->cat.icon = pw_assets_art(PW_ART_SETTINGS);
    s->cat.fallback = PW_ICON_GEAR;
    s->cat.label = pw_tr(PW_S_SETTINGS);
    s->cat.subtitle = 0;
    s->rows[SET_STYLE].label = pw_tr(PW_S_SET_STYLE);
    s->rows[SET_STYLE].value = pw_tr(g_style_names[cfg->style % PW_STYLE_COUNT]);
    s->rows[SET_STYLE].toggle = -1;
    s->rows[SET_FAST].label = pw_tr(PW_S_SET_FAST);
    s->rows[SET_FAST].value = pw_tr(cfg->fast ? PW_S_ON : PW_S_OFF);
    s->rows[SET_FAST].toggle = cfg->fast != 0;
    s->rows[SET_STATUS].label = pw_tr(PW_S_SET_STATUS);
    s->rows[SET_STATUS].value = pw_tr(cfg->status ? PW_S_ON : PW_S_OFF);
    s->rows[SET_STATUS].toggle = cfg->status != 0;
    s->rows[SET_LANGUAGE].label = pw_tr(PW_S_SET_LANGUAGE);
    s->rows[SET_LANGUAGE].value = language_name(language_index());
    s->rows[SET_LANGUAGE].toggle = -1;
}

/* The option popup: returns the chosen index, or -1 for Back. */
static int pick(pw_ui *ui, const screen *s, int sel, const char *const *options, int n, int cur)
{
    unsigned int t_open = pw_ui_now_ms();

    pw_ui_flush_input(ui);
    for (;;) {
        unsigned int ev, now = pw_ui_now_ms();

        pw_view_settings(pw_fb_begin(256, now), ui, &s->cat, s->rows, SET_COUNT, sel, options, n,
                         cur, pw_ramp(now, t_open, 180));
        pw_fb_present();
        ev = pw_ui_poll(ui);
        if (ev & PSP_CTRL_UP) {
            if (cur > 0)
                cur--;
        } else if (ev & PSP_CTRL_DOWN) {
            if (cur < n - 1)
                cur++;
        } else if (ev & ui->enter) {
            return cur;
        } else if (ev & (ui->back | PSP_CTRL_HOME)) {
            return -1;
        }
    }
}

/*
 * The hub's Settings screen (CPL-REQ-006): the style, fast save and load, the clock and battery
 * corner, the language. Changes apply at once; config.txt / language.txt are written when it
 * closes.
 */
static void settings_screen(pw_ui *ui)
{
    static screen s;
    pw_config *cfg = pw_config_get();
    int sel = 0, changed = 0, lang_failed = 0;

    build(&s);
    pw_ui_flush_input(ui);
    for (;;) {
        unsigned int ev;

        pw_view_settings(pw_fb_begin(256, pw_ui_now_ms()), ui, &s.cat, s.rows, SET_COUNT, sel, 0, 0,
                         -1, 256);
        pw_fb_present();
        ev = pw_ui_poll(ui);
        if (ev & PSP_CTRL_UP) {
            if (sel > 0)
                sel--;
        } else if (ev & PSP_CTRL_DOWN) {
            if (sel < SET_COUNT - 1)
                sel++;
        } else if (ev & (ui->back | PSP_CTRL_HOME)) {
            break;
        } else if (ev & ui->enter) {
            if (sel == SET_STYLE) {
                const char *names[PW_STYLE_COUNT];
                int k;

                for (int i = 0; i < PW_STYLE_COUNT; i++)
                    names[i] = pw_tr(g_style_names[i]);
                k = pick(ui, &s, sel, names, PW_STYLE_COUNT, cfg->style % PW_STYLE_COUNT);
                if (k >= 0 && k != cfg->style) {
                    cfg->style = k;
                    ui->style = k;      /* this very screen redraws in the new style */
                    pw_fb_set_style(k); /* and on its backdrop */
                    changed = 1;
                }
            } else if (sel == SET_LANGUAGE) {
                const char *names[16];
                int k;

                for (int i = 0; i < LANGS; i++)
                    names[i] = language_name(i);
                k = pick(ui, &s, sel, names, LANGS, language_index());
                if (k >= 0 && k != language_index())
                    lang_failed |= pw_lang_choose(pw_lang_names[k].code) < 0;
            } else {
                const char *names[2] = {pw_tr(PW_S_ON), pw_tr(PW_S_OFF)};
                int *v = sel == SET_FAST ? &cfg->fast : &cfg->status;
                int k = pick(ui, &s, sel, names, 2, *v ? 0 : 1);

                if (k >= 0 && (k == 0) != (*v != 0)) {
                    *v = k == 0;
                    changed = 1;
                    if (sel == SET_FAST)
                        pw_engine_set_fast(*v);
                }
            }
            build(&s);
            pw_ui_flush_input(ui);
        }
    }
    if (changed) {
        char path[256];

        config_path(path, sizeof(path));
        if (pw_log_card_blocked() || pw_config_save(path) < 0)
            lang_failed = 1;
        else
            pw_log("settings: style %d, fast %d, status %d saved", cfg->style, cfg->fast,
                   cfg->status);
    }
    if (lang_failed)
        pw_dialog_message(ui, pw_tr(PW_S_SETTINGS_NOT_SAVED), pw_tr(PW_S_E_IO));
    pw_ui_flush_input(ui);
}

static int settings_items(pw_cpl_item *out, int max)
{
    if (max < 1)
        return 0;
    out[0].art = PW_ART_SETTINGS;
    out[0].glow = PW_ART_SETTINGS_GLOW;
    out[0].fallback = PW_ICON_GEAR;
    out[0].title = pw_tr(PW_S_SETTINGS);
    out[0].enabled = 1;
    return 1;
}

static int settings_run(int index, const pw_cpl_env *env)
{
    (void)index;
    settings_screen(env->ui);
    return PW_CPL_STAY; /* the hub rebuilds its rows: the language may have changed */
}

const pw_complication pw_cpl_settings = {
    .id = "settings", .init = settings_init, .items = settings_items, .run = settings_run};
