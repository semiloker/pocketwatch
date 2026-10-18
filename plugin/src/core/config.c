// SPDX-License-Identifier: GPL-3.0-or-later
#include <stddef.h>
#include <string.h>

#include "core/config.h"
#include "core/platform.h"
#include "stb_sprintf.h"

#define FILE_MAX 1024

static pw_config g_config = {1, 1, 0, 0, 0, 0};

const char *const pw_config_style_names[3] = {"xmb", "modern", "minimal"};
const char *const pw_config_start_names[2] = {"xmb", "home"};
const char *const pw_config_sort_names[2] = {"name", "recent"};
const char *const pw_config_slide_names[3] = {"normal", "fast", "slow"};

void pw_config_defaults(pw_config *c)
{
    c->fast = 1;
    c->status = 1;
    c->style = 0;
    c->start = 0;
    c->sort = 0;
    c->slide = 0;
}

static int name_index(const char *const *names, int count, const char *v, unsigned int n)
{
    for (int i = 0; i < count; i++)
        if (strlen(names[i]) == n && strncmp(v, names[i], n) == 0)
            return i;
    return -1;
}

static int style(const char *v, unsigned int n)
{
    return name_index(pw_config_style_names, 3, v, n);
}

static int start(const char *v, unsigned int n)
{
    return name_index(pw_config_start_names, 2, v, n);
}

static int sort(const char *v, unsigned int n)
{
    return name_index(pw_config_sort_names, 2, v, n);
}

static int slide(const char *v, unsigned int n)
{
    return name_index(pw_config_slide_names, 3, v, n);
}

/* "on"/"off", "1"/"0", "yes"/"no", "true"/"false"; -1 for anything else. */
static int boolean(const char *v, unsigned int n)
{
    static const char *const yes[] = {"on", "1", "yes", "true"};
    static const char *const no[] = {"off", "0", "no", "false"};

    for (unsigned int i = 0; i < 4; i++) {
        if (strlen(yes[i]) == n && strncmp(v, yes[i], n) == 0)
            return 1;
        if (strlen(no[i]) == n && strncmp(v, no[i], n) == 0)
            return 0;
    }
    return -1;
}

static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

int pw_config_parse(pw_config *c, const char *text, unsigned int len)
{
    static const struct {
        const char *key;
        unsigned int offset;
        int (*value)(const char *v, unsigned int n);
    } keys[] = {
        {"fast", (unsigned int)offsetof(pw_config, fast), boolean},
        {"status", (unsigned int)offsetof(pw_config, status), boolean},
        {"style", (unsigned int)offsetof(pw_config, style), style},
        {"start", (unsigned int)offsetof(pw_config, start), start},
        {"sort", (unsigned int)offsetof(pw_config, sort), sort},
        {"slideshow", (unsigned int)offsetof(pw_config, slide), slide},
    };
    const char *p = text, *end = text + len;
    int taken = 0;

    while (p < end) {
        const char *nl = memchr(p, '\n', (unsigned int)(end - p)), *e = nl ? nl : end;
        const char *eq = memchr(p, '=', (unsigned int)(e - p));

        if (*p != '#' && eq) {
            const char *k = p, *ke = eq, *v = eq + 1, *ve = e;

            while (k < ke && is_space(*k))
                k++;
            while (ke > k && is_space(ke[-1]))
                ke--;
            while (v < ve && is_space(*v))
                v++;
            while (ve > v && is_space(ve[-1]))
                ve--;
            for (unsigned int i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
                if (strlen(keys[i].key) == (unsigned int)(ke - k) &&
                    strncmp(k, keys[i].key, (unsigned int)(ke - k)) == 0) {
                    int b = keys[i].value(v, (unsigned int)(ve - v));

                    if (b >= 0) {
                        *(int *)((char *)c + keys[i].offset) = b;
                        taken++;
                    }
                }
        }
        p = nl ? nl + 1 : end;
    }
    return taken;
}

int pw_config_format(const pw_config *c, char *out, unsigned int cap)
{
    int n =
        stbsp_snprintf(out, (int)cap,
                       "# Pocketwatch settings (XMB -> Settings -> Pocketwatch writes this file)\n"
                       "fast = %s\n"
                       "status = %s\n"
                       "style = %s\n"
                       "start = %s\n"
                       "sort = %s\n"
                       "slideshow = %s\n",
                       c->fast ? "on" : "off", c->status ? "on" : "off",
                       pw_config_style_names[c->style >= 0 && c->style < 3 ? c->style : 0],
                       pw_config_start_names[c->start == 1], pw_config_sort_names[c->sort == 1],
                       pw_config_slide_names[c->slide >= 0 && c->slide < 3 ? c->slide : 0]);

    return n >= 0 && (unsigned int)n < cap ? n : -1;
}

pw_config *pw_config_get(void)
{
    return &g_config;
}

int pw_config_load(const char *path)
{
    char text[FILE_MAX];
    int n;

    pw_config_defaults(&g_config);
    n = pw_file_read(path, text, sizeof(text));
    return n > 0 ? pw_config_parse(&g_config, text, (unsigned int)n) : 0;
}

int pw_config_save(const char *path)
{
    char text[256];
    int n = pw_config_format(&g_config, text, sizeof(text));

    if (n < 0)
        return -1;
    return pw_file_write(path, text, (unsigned int)n, 0) == n ? 0 : -1;
}
