// SPDX-License-Identifier: GPL-3.0-or-later
#include <stddef.h>
#include <string.h>

#include "core/platform.h"
#include "hcfg.h"
#include "stb_sprintf.h"
#include "ui/draw.h"

#define FILE_MAX 1024

static hm_cfg g_cfg = {0, 0, 0, 0, 1, 1, 1, 0, 0};

const char *const hm_cfg_accent_names[HM_ACCENTS] = {"blue",  "violet", "pink",  "red",     "orange",
                                                     "gold",  "green",  "teal",  "graphite"};
const char *const hm_cfg_theme_names[4] = {"dark", "glass", "oled", "paper"};
const char *const hm_cfg_bg_names[HM_BGS] = {"auto", "sky", "gradient", "plain", "wallpaper"};
const char *const hm_cfg_motion_names[HM_MOTIONS] = {"normal", "fast", "calm"};
const char *const hm_cfg_repeat_names[3] = {"off", "all", "one"};

static const unsigned int k_accent[HM_ACCENTS] = {
    PW_RGB(0, 114, 206),  PW_RGB(128, 84, 230), PW_RGB(232, 72, 140),
    PW_RGB(226, 58, 58),  PW_RGB(240, 130, 30), PW_RGB(212, 166, 24),
    PW_RGB(40, 168, 90),  PW_RGB(0, 158, 170),  PW_RGB(118, 126, 146),
};

void hm_cfg_defaults(hm_cfg *c)
{
    memset(c, 0, sizeof(*c));
    c->wave = 1;
    c->percent = 1;
    c->repeat = HM_REPEAT_ALL;
}

unsigned int hm_cfg_accent(int i)
{
    return k_accent[i >= 0 && i < HM_ACCENTS ? i : 0];
}

int hm_cfg_tau(int motion, int tau_ms)
{
    return motion == HM_MOTION_FAST ? tau_ms * 3 / 5 : motion == HM_MOTION_CALM ? tau_ms * 8 / 5
                                                                                : tau_ms;
}

static int name_index(const char *const *names, int count, const char *v, unsigned int n)
{
    for (int i = 0; i < count; i++)
        if (strlen(names[i]) == n && strncmp(v, names[i], n) == 0)
            return i;
    return -1;
}

static int boolean(const char *v, unsigned int n)
{
    if ((n == 2 && strncmp(v, "on", 2) == 0) || (n == 1 && v[0] == '1'))
        return 1;
    if ((n == 3 && strncmp(v, "off", 3) == 0) || (n == 1 && v[0] == '0'))
        return 0;
    return -1;
}

static int number(const char *v, unsigned int n, int lo, int hi)
{
    int x = 0;

    if (n == 0 || n > 3)
        return -1;
    for (unsigned int i = 0; i < n; i++) {
        if (v[i] < '0' || v[i] > '9')
            return -1;
        x = x * 10 + (v[i] - '0');
    }
    return x >= lo && x <= hi ? x : -1;
}

static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

/* One "key = value": taken into c (1) or not (0). */
static int take(hm_cfg *c, const char *k, unsigned int kn, const char *v, unsigned int vn)
{
    int x = -1;

#define KEY(s) (kn == sizeof(s) - 1 && strncmp(k, s, kn) == 0)
    if (KEY("accent") && (x = name_index(hm_cfg_accent_names, HM_ACCENTS, v, vn)) >= 0)
        c->accent = x;
    else if (KEY("theme") && (x = name_index(hm_cfg_theme_names, 4, v, vn)) >= 0)
        c->theme = x;
    else if (KEY("background") && (x = name_index(hm_cfg_bg_names, HM_BGS, v, vn)) >= 0)
        c->bg = x;
    else if (KEY("sky") && ((vn == 6 && strncmp(v, "system", 6) == 0 && (x = 0) == 0) ||
                            (x = number(v, vn, 1, 12)) >= 0))
        c->sky = x;
    else if (KEY("wave") && (x = boolean(v, vn)) >= 0)
        c->wave = x;
    else if (KEY("percent") && (x = boolean(v, vn)) >= 0)
        c->percent = x;
    else if (KEY("repeat") && (x = name_index(hm_cfg_repeat_names, 3, v, vn)) >= 0)
        c->repeat = x;
    else if (KEY("shuffle") && (x = boolean(v, vn)) >= 0)
        c->shuffle = x;
    else if (KEY("motion") && (x = name_index(hm_cfg_motion_names, HM_MOTIONS, v, vn)) >= 0)
        c->motion = x;
#undef KEY
    return x >= 0;
}

int hm_cfg_parse(hm_cfg *c, const char *text, unsigned int len)
{
    unsigned int i = 0;
    int taken = 0;

    while (i < len) {
        unsigned int ls = i, le, eq, ks, ke, vs, ve;

        while (i < len && text[i] != '\n')
            i++;
        le = i++;
        for (eq = ls; eq < le && text[eq] != '='; eq++)
            ;
        if (eq == le || text[ls] == '#')
            continue;
        for (ks = ls; ks < eq && is_space(text[ks]); ks++)
            ;
        for (ke = eq; ke > ks && is_space(text[ke - 1]); ke--)
            ;
        for (vs = eq + 1; vs < le && is_space(text[vs]); vs++)
            ;
        for (ve = le; ve > vs && is_space(text[ve - 1]); ve--)
            ;
        taken += take(c, text + ks, ke - ks, text + vs, ve - vs);
    }
    return taken;
}

int hm_cfg_format(const hm_cfg *c, char *out, unsigned int cap)
{
    char sky[8];
    int n;

    if (c->sky >= 1 && c->sky <= 12)
        stbsp_snprintf(sky, sizeof(sky), "%d", c->sky);
    else
        stbsp_snprintf(sky, sizeof(sky), "system");
    n = stbsp_snprintf(out, (int)cap,
                       "# Pocketwatch Home (its Settings write this file)\n"
                       "accent = %s\ntheme = %s\nbackground = %s\nsky = %s\nwave = %s\n"
                       "percent = %s\nrepeat = %s\nshuffle = %s\nmotion = %s\n",
                       hm_cfg_accent_names[c->accent >= 0 && c->accent < HM_ACCENTS ? c->accent
                                                                                     : 0],
                       hm_cfg_theme_names[c->theme >= 0 && c->theme < 4 ? c->theme : 0],
                       hm_cfg_bg_names[c->bg >= 0 && c->bg < HM_BGS ? c->bg : 0], sky,
                       c->wave ? "on" : "off", c->percent ? "on" : "off",
                       hm_cfg_repeat_names[c->repeat >= 0 && c->repeat < 3 ? c->repeat : 1],
                       c->shuffle ? "on" : "off",
                       hm_cfg_motion_names[c->motion >= 0 && c->motion < HM_MOTIONS ? c->motion
                                                                                    : 0]);
    return n > 0 && (unsigned int)n < cap ? n : -1;
}

hm_cfg *hm_cfg_get(void)
{
    return &g_cfg;
}

int hm_cfg_load(const char *path)
{
    char buf[FILE_MAX];
    int n;

    hm_cfg_defaults(&g_cfg);
    n = pw_file_read(path, buf, sizeof(buf));
    if (n <= 0)
        return n;
    return hm_cfg_parse(&g_cfg, buf, (unsigned int)n);
}

int hm_cfg_save(const char *path)
{
    char buf[FILE_MAX];
    int n = hm_cfg_format(&g_cfg, buf, sizeof(buf));

    if (n < 0)
        return -1;
    return pw_file_write(path, buf, (unsigned int)n, 0) == n ? 0 : -1;
}
