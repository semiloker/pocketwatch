// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspkernel.h>
#include <pspthreadman.h>
#include <pspthreadman_kernel.h>
#include <string.h>

#include "core/log.h"
#include "stb_sprintf.h"
#include "ui/cpuwatch.h"

#define WATCH_MAX 96
#define TOP       6

/* At the start only ids and run times (kernel RAM is scarce); the names come at the end. */
static struct {
    SceUID id;
    unsigned int us;
} g_start[WATCH_MAX];
static int g_n;
static unsigned int g_t0;

static int thread_ids(SceUID *ids)
{
    int count = 0;

    if (sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, ids, WATCH_MAX, &count) < 0)
        return 0;
    return count > WATCH_MAX ? WATCH_MAX : count;
}

static int refer(SceUID id, SceKernelThreadInfo *info)
{
    memset(info, 0, sizeof(*info));
    info->size = sizeof(*info);
    return sceKernelReferThreadStatus(id, info);
}

void pw_cpuwatch_start(void)
{
    SceUID ids[WATCH_MAX];
    int count = thread_ids(ids);

    g_n = 0;
    for (int i = 0; i < count; i++) {
        SceKernelThreadInfo info;

        if (refer(ids[i], &info) < 0)
            continue;
        g_start[g_n].id = ids[i];
        g_start[g_n].us = info.runClocks.low; /* microseconds; the low word covers a session */
        g_n++;
    }
    g_t0 = sceKernelGetSystemTimeLow();
}

void pw_cpuwatch_stop(const char *what)
{
    struct {
        char name[32];
        unsigned int us;
    } top[TOP];
    SceUID ids[WATCH_MAX];
    char line[256];
    unsigned int span = sceKernelGetSystemTimeLow() - g_t0;
    int count = thread_ids(ids), len, k = 0;

    if (g_n == 0 || count == 0 || span == 0)
        return;
    memset(top, 0, sizeof(top));
    for (int i = 0; i < count; i++) {
        SceKernelThreadInfo info;
        unsigned int used = 0;

        if (refer(ids[i], &info) < 0)
            continue;
        for (int j = 0; j < g_n; j++)
            if (g_start[j].id == ids[i]) {
                used = info.runClocks.low - g_start[j].us;
                break;
            }
        for (int t = 0; t < TOP; t++) /* insert into the top list, biggest first */
            if (used > top[t].us) {
                memmove(&top[t + 1], &top[t], (unsigned int)(TOP - 1 - t) * sizeof(top[0]));
                strncpy(top[t].name, info.name, sizeof(top[t].name) - 1);
                top[t].name[sizeof(top[t].name) - 1] = 0;
                top[t].us = used;
                break;
            }
    }
    len = stbsp_snprintf(line, sizeof(line), "cpu: %s, %u ms:", what, span / 1000);
    for (int t = 0; t < TOP && top[t].us && len < (int)sizeof(line) - 40; t++, k++)
        len += stbsp_snprintf(line + len, (int)sizeof(line) - len, " %s %u%%", top[t].name,
                              top[t].us / (span / 100 + 1));
    if (k)
        pw_log("%s", line);
}
