// SPDX-License-Identifier: GPL-3.0-or-later
#include <malloc.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <stdlib.h>
#include <string.h>

#include "complications/states/sfo.h"
#include "hw.h"
#include "savedata.h"
#include "stb_sprintf.h"

#define SAVEDATA "ms0:/PSP/SAVEDATA"
#define SFO_MAX  4096

static unsigned long long stamp(const ScePspDateTime *t)
{
    return ((((unsigned long long)t->year * 13 + t->month) * 32 + t->day) * 24 + t->hour) * 3600 +
           (unsigned long long)t->minute * 60 + t->second;
}

static void sfo_string(const unsigned char *sfo, int len, const char *key, char *out, int cap)
{
    unsigned int off, n;

    out[0] = 0;
    if (pw_sfo_find(sfo, (unsigned int)len, key, &off, &n) < 0)
        return;
    if (n >= (unsigned int)cap)
        n = (unsigned int)cap - 1;
    memcpy(out, sfo + off, n);
    out[n] = 0;
}

/* The folder's files: their count and size. */
static void measure(const char *dir, hm_save *s)
{
    SceUID dd = sceIoDopen(dir);
    SceIoDirent e;
    unsigned long long bytes = 0;

    s->files = 0;
    if (dd < 0)
        return;
    memset(&e, 0, sizeof(e));
    while (sceIoDread(dd, &e) > 0) {
        if (!FIO_S_ISDIR(e.d_stat.st_mode)) {
            bytes += (unsigned long long)e.d_stat.st_size;
            s->files++;
        }
        memset(&e, 0, sizeof(e));
    }
    sceIoDclose(dd);
    s->size_kb = (unsigned int)((bytes + 1023) >> 10);
}

static void one(hm_save *s, const char *name, const SceIoStat *st)
{
    char dir[96], path[128];
    unsigned char *sfo = malloc(SFO_MAX);
    int len;

    memset(s, 0, sizeof(*s));
    stbsp_snprintf(s->dir, sizeof(s->dir), "%s", name);
    stbsp_snprintf(dir, sizeof(dir), SAVEDATA "/%s", name);
    stbsp_snprintf(path, sizeof(path), "%s/PARAM.SFO", dir);
    if (sfo && (len = hm_hw_read_file(path, sfo, SFO_MAX)) > 0) {
        char detail[512];

        sfo_string(sfo, len, "SAVEDATA_TITLE", s->title, sizeof(s->title));
        sfo_string(sfo, len, "TITLE", s->game, sizeof(s->game));
        sfo_string(sfo, len, "SAVEDATA_DETAIL", detail, sizeof(detail));
        hm_saves_first_line(detail, (unsigned int)strlen(detail), s->detail, sizeof(s->detail));
    }
    free(sfo);
    if (!s->title[0])
        stbsp_snprintf(s->title, sizeof(s->title), "%s", s->game[0] ? s->game : name);
    measure(dir, s);
    s->stamp = stamp(&st->sce_st_mtime);
    hm_hw_format_time(&st->sce_st_mtime, s->when, sizeof(s->when));
    if (s->size_kb >= 1024)
        stbsp_snprintf(s->meta, sizeof(s->meta), "%s   %u.%u MB", s->when, s->size_kb / 1024,
                       s->size_kb % 1024 * 10 / 1024);
    else
        stbsp_snprintf(s->meta, sizeof(s->meta), "%s   %u KB", s->when, s->size_kb);
}

int hm_saves_list(const char *disc_id, hm_save *out, int max)
{
    SceUID dd = sceIoDopen(SAVEDATA);
    SceIoDirent e;
    int n = 0, idn = (int)strlen(disc_id);

    if (dd < 0 || idn == 0) {
        if (dd >= 0)
            sceIoDclose(dd);
        return 0;
    }
    memset(&e, 0, sizeof(e));
    while (sceIoDread(dd, &e) > 0 && n < max) {
        if (FIO_S_ISDIR(e.d_stat.st_mode) && strncmp(e.d_name, disc_id, (size_t)idn) == 0) {
            SceIoStat st;
            char dir[96];
            int k = n++;

            stbsp_snprintf(dir, sizeof(dir), SAVEDATA "/%s", e.d_name);
            if (sceIoGetstat(dir, &st) < 0)
                st = e.d_stat;
            one(&out[k], e.d_name, &st);
            while (k > 0 && out[k - 1].stamp < out[k].stamp) { /* newest first */
                hm_save t = out[k];

                out[k] = out[k - 1];
                out[k - 1] = t;
                k--;
            }
        }
        memset(&e, 0, sizeof(e));
    }
    sceIoDclose(dd);
    return n;
}

void hm_saves_path(const hm_save *s, const char *file, char *out, int cap)
{
    stbsp_snprintf(out, cap, SAVEDATA "/%s/%s", s->dir, file);
}

static int remove_dir(const char *dir)
{
    SceUID dd = sceIoDopen(dir);
    SceIoDirent e;
    char names[32][64];
    int n = 0, failed = 0;

    if (dd < 0)
        return -1;
    memset(&e, 0, sizeof(e));
    while (sceIoDread(dd, &e) > 0 && n < 32) { /* the names first: no removing while listing */
        if (!FIO_S_ISDIR(e.d_stat.st_mode) && e.d_name[0] != '.')
            stbsp_snprintf(names[n++], sizeof(names[0]), "%s", e.d_name);
        memset(&e, 0, sizeof(e));
    }
    sceIoDclose(dd);
    for (int i = 0; i < n; i++) {
        char path[160];

        stbsp_snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        failed |= sceIoRemove(path) < 0;
    }
    return sceIoRmdir(dir) < 0 || failed ? -1 : 0;
}

int hm_saves_delete(const hm_save *s)
{
    char dir[96];

    if (!s->dir[0] || strchr(s->dir, '/') || s->dir[0] == '.')
        return -1;
    stbsp_snprintf(dir, sizeof(dir), SAVEDATA "/%s", s->dir);
    return remove_dir(dir);
}

int hm_saves_delete_state(const char *disc_id, int slot)
{
    char dir[96];

    if (!disc_id[0] || slot < 1 || slot > 99)
        return -1;
    stbsp_snprintf(dir, sizeof(dir), SAVEDATA "/PW%s%02d", disc_id, slot);
    return remove_dir(dir);
}
