// SPDX-License-Identifier: GPL-3.0-or-later
#include <malloc.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_gu.h"
#include "hm_image.h"
#include "home_view.h"
#include "hw.h"
#include "states.h"
#include "stb_sprintf.h"

#define SAVEDATA "ms0:/PSP/SAVEDATA/"
#define CONTINUE "ms0:/seplugins/pocketwatch/continue.txt"
#define ICON_MAX (128 * 1024)

static unsigned int *load_icon(const char *dir)
{
    char path[96];
    int len, w = 0, h = 0;
    unsigned char *png;
    unsigned int *px, *icon = 0;
    SceIoStat st;

    stbsp_snprintf(path, sizeof(path), "%s/ICON0.PNG", dir);
    if (sceIoGetstat(path, &st) < 0 || st.st_size <= 0 || st.st_size > ICON_MAX ||
        !(png = malloc((size_t)st.st_size)))
        return 0;
    len = hm_hw_read_file(path, png, (unsigned int)st.st_size);
    px = len > 0 ? hm_png_decode(png, len, &w, &h) : 0;
    free(png);
    if (!px)
        return 0;
    if ((icon = memalign(16, HM_ICON_W * HM_ICON_H * 4)) != 0) {
        hm_image_fit(px, w, h, icon, HM_ICON_W, HM_ICON_H);
        hm_gu_written(icon, HM_ICON_W * HM_ICON_H * 4);
    }
    hm_image_free(px);
    return icon;
}

static unsigned long long stamp(const ScePspDateTime *t)
{
    return ((((unsigned long long)t->year * 13 + t->month) * 32 + t->day) * 24 + t->hour) * 3600 +
           (unsigned long long)t->minute * 60 + t->second;
}

int hm_states_list(const char *disc_id, hm_state *out)
{
    unsigned long long when[HM_STATES_MAX];
    int n = 0;

    for (int slot = 1; slot <= HM_STATES_MAX; slot++) {
        char dir[64], file[96];
        SceIoStat st;
        int k;

        stbsp_snprintf(dir, sizeof(dir), SAVEDATA "PW%s%02d", disc_id, slot);
        stbsp_snprintf(file, sizeof(file), "%s/STATE.BIN", dir);
        if (sceIoGetstat(file, &st) < 0)
            continue;
        k = n++;
        while (k > 0 && when[k - 1] < stamp(&st.sce_st_mtime)) { /* newest first */
            out[k] = out[k - 1];
            when[k] = when[k - 1];
            k--;
        }
        out[k].slot = slot;
        when[k] = stamp(&st.sce_st_mtime);
        hm_hw_format_time(&st.sce_st_mtime, out[k].when, sizeof(out[k].when));
        out[k].icon = load_icon(dir);
    }
    return n;
}

void hm_states_free(hm_state *s, int n)
{
    for (int i = 0; i < n; i++) {
        free(s[i].icon);
        s[i].icon = 0;
    }
}

int hm_states_continue(const char *disc_id, int slot)
{
    char line[32];
    SceUID fd = sceIoOpen(CONTINUE, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    int n, ok;

    if (fd < 0)
        return -1;
    n = stbsp_snprintf(line, sizeof(line), "%s %d\n", disc_id, slot);
    ok = sceIoWrite(fd, line, (unsigned int)n) == n;
    sceIoClose(fd);
    return ok ? 0 : -1;
}
