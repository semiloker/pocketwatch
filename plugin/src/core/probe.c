// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>
#include <pspge.h>
#include <pspinit.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <pspmodulemgr.h>
#include <pspsysmem_kernel.h>
#include <pspthreadman.h>
#include <string.h>

#include "core/log.h"
#include "core/probe.h"

#define PROBE_COMBO      (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_SELECT | PSP_CTRL_UP)
#define PROBE_MAX_IDS    128
#define PROBE_PARTITIONS 12

static SceUID g_ids[PROBE_MAX_IDS];
static int g_dump_count;

/* Game ID from the disc header ("ULUS-10336|..."); empty for homebrew or when not mounted. */
static void read_game_id(char *out, unsigned int cap)
{
    SceUID fd = sceIoOpen("disc0:/UMD_DATA.BIN", PSP_O_RDONLY, 0);
    int n = 0;

    out[0] = '\0';
    if (fd < 0)
        return;
    n = sceIoRead(fd, out, cap - 1);
    sceIoClose(fd);
    if (n < 0)
        n = 0;
    out[n] = '\0';
    for (int i = 0; i < n; i++) {
        if (out[i] == '|') {
            out[i] = '\0';
            break;
        }
    }
}

static void dump_system(void)
{
    char game_id[16];
    const char *init_file = sceKernelInitFileName();

    read_game_id(game_id, sizeof(game_id));
    pw_log("probe: model=%d devkit=0x%08X apitype=0x%X game=%s init=%s", sceKernelGetModel(),
           sceKernelDevkitVersion(), sceKernelInitApitype(), game_id[0] ? game_id : "-",
           init_file ? init_file : "-");
    pw_log("probe: edram=%u KB", sceGeEdramGetSize() / 1024);
}

static void dump_partitions(void)
{
    for (int pid = 1; pid <= PROBE_PARTITIONS; pid++) {
        PspSysmemPartitionInfo info;

        memset(&info, 0, sizeof(info));
        info.size = sizeof(info);
        if (sceKernelQueryMemoryPartitionInfo(pid, &info) < 0)
            continue;
        pw_log("probe: partition %2d start=0x%08X size=%6u KB attr=0x%X free=%6u KB maxfree=%6u KB",
               pid, info.startaddr, info.memsize / 1024, info.attr,
               (unsigned int)sceKernelPartitionTotalFreeMemSize(pid) / 1024,
               (unsigned int)sceKernelPartitionMaxFreeMemSize(pid) / 1024);
    }
}

static void dump_modules(void)
{
    int count = 0;

    if (sceKernelGetModuleIdList(g_ids, sizeof(g_ids), &count) < 0)
        return;
    if (count > PROBE_MAX_IDS)
        count = PROBE_MAX_IDS;
    pw_log("probe: %d module(s)", count);
    for (int i = 0; i < count; i++) {
        SceKernelModuleInfo m;

        memset(&m, 0, sizeof(m));
        m.size = sizeof(m);
        if (sceKernelQueryModuleInfo(g_ids[i], &m) < 0)
            continue;
        pw_log("probe: module %-27.27s attr=0x%04X text=0x%08X+%u data=%u bss=%u seg0=0x%08X+%u",
               m.name, m.attribute, m.text_addr, m.text_size, m.data_size, m.bss_size,
               (unsigned int)m.segmentaddr[0], (unsigned int)m.segmentsize[0]);
    }
}

static void dump_threads(void)
{
    int count = 0;

    if (sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, g_ids, PROBE_MAX_IDS, &count) < 0)
        return;
    if (count > PROBE_MAX_IDS)
        count = PROBE_MAX_IDS;
    pw_log("probe: %d thread(s)", count);
    for (int i = 0; i < count; i++) {
        SceKernelThreadInfo t;

        memset(&t, 0, sizeof(t));
        t.size = sizeof(t);
        if (sceKernelReferThreadStatus(g_ids[i], &t) < 0)
            continue;
        pw_log("probe: thread %-31.31s attr=0x%08X prio=%3d status=0x%02X wait=%d stack=0x%08X+%d",
               t.name, t.attr, t.currentPriority, t.status, t.waitType, (unsigned int)t.stack,
               t.stackSize);
    }
}

void pw_probe_boot(void)
{
    dump_system();
    dump_partitions();
}

static int probe_thread(SceSize args, void *argp)
{
    SceCtrlData pad;

    (void)args;
    (void)argp;
    for (;;) {
        sceKernelDelayThread(50 * 1000);
        if (sceCtrlPeekBufferPositive(&pad, 1) < 0 || (pad.Buttons & PROBE_COMBO) != PROBE_COMBO)
            continue;

        pw_log("probe: ---- dump %d ----", ++g_dump_count);
        dump_system();
        dump_partitions();
        dump_modules();
        dump_threads();
        pw_log("probe: ---- end of dump %d ----", g_dump_count);

        /* One dump per press. */
        do {
            sceKernelDelayThread(100 * 1000);
            sceCtrlPeekBufferPositive(&pad, 1);
        } while ((pad.Buttons & PROBE_COMBO) == PROBE_COMBO);
    }
    return 0;
}

void pw_probe_start(void)
{
    SceUID thid = sceKernelCreateThread("PocketwatchProbe", probe_thread, 0x30, 0x2000, 0, NULL);

    if (thid >= 0)
        sceKernelStartThread(thid, 0, NULL);
}
