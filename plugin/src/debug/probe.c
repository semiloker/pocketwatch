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
#include "debug/probe.h"
#include "debug/spike_savedata.h"
#include "debug/spike_suspend.h"

#define COMBO_BASE           (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_SELECT)
#define PROBE_COMBO          (COMBO_BASE | PSP_CTRL_UP)
#define SPIKE_SAVEDATA_COMBO (COMBO_BASE | PSP_CTRL_LEFT)
#define SPIKE_SUSPEND_COMBO  (COMBO_BASE | PSP_CTRL_RIGHT)
#define PROBE_MAX_IDS        128
#define PROBE_PARTITIONS     12

static SceUID g_ids[PROBE_MAX_IDS];
static int g_dump_count;
static SceUID g_probe_thread = -1;
static volatile int g_probe_stop;

/* Retail disc/ISO apitypes (ARK-4 runlevel "umd"); disc0: is only mounted for these. */
static int is_disc_apitype(int apitype)
{
    return apitype == 0x120 || (apitype >= 0x123 && apitype <= 0x126) || apitype == 0x130 ||
           apitype == 0x160 || (apitype >= 0x110 && apitype <= 0x115);
}

/* Game ID from the disc header ("ULUS-10336|..."); empty for homebrew or when not mounted. */
static void read_game_id(char *out, unsigned int cap)
{
    SceUID fd;
    int n = 0;

    out[0] = '\0';
    if (!is_disc_apitype(sceKernelInitApitype()))
        return;
    fd = sceIoOpen("disc0:/UMD_DATA.BIN", PSP_O_RDONLY, 0);
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
    for (int i = 0; i < count && !g_probe_stop; i++) {
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
    for (int i = 0; i < count && !g_probe_stop; i++) {
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

/* One action per press: wait until the combo is let go. */
static void wait_release(unsigned int combo)
{
    SceCtrlData pad;

    do {
        sceKernelDelayThread(100 * 1000);
        sceCtrlPeekBufferPositive(&pad, 1);
    } while (!g_probe_stop && (pad.Buttons & combo) == combo);
}

static int probe_thread(SceSize args, void *argp)
{
    SceCtrlData pad;

    (void)args;
    (void)argp;
    while (!g_probe_stop) {
        sceKernelDelayThread(50 * 1000);
        if (sceCtrlPeekBufferPositive(&pad, 1) < 0)
            continue;

        if ((pad.Buttons & PROBE_COMBO) == PROBE_COMBO) {
            pw_log("probe: ---- dump %d ----", ++g_dump_count);
            dump_system();
            dump_partitions();
            dump_modules();
            dump_threads();
            pw_log("probe: ---- end of dump %d ----", g_dump_count);
            wait_release(PROBE_COMBO);
        } else if ((pad.Buttons & SPIKE_SAVEDATA_COMBO) == SPIKE_SAVEDATA_COMBO) {
            pw_spike_savedata();
            wait_release(SPIKE_SAVEDATA_COMBO);
        } else if ((pad.Buttons & SPIKE_SUSPEND_COMBO) == SPIKE_SUSPEND_COMBO) {
            wait_release(SPIKE_SUSPEND_COMBO); /* do not hold buttons across the sleep */
            pw_spike_suspend();
        }
    }
    return 0;
}

void pw_probe_start(void)
{
    int ret = pw_spike_suspend_init();

    if (ret < 0)
        pw_log("probe: sysevent handler registration failed 0x%08X", ret);
    g_probe_stop = 0;
    g_probe_thread = sceKernelCreateThread("PocketwatchProbe", probe_thread, 0x30, 0x2000, 0, NULL);
    if (g_probe_thread >= 0)
        sceKernelStartThread(g_probe_thread, 0, NULL);
}

void pw_probe_stop(void)
{
    SceUInt timeout = 500 * 1000;

    if (g_probe_thread < 0)
        return;
    g_probe_stop = 1;
    if (sceKernelWaitThreadEnd(g_probe_thread, &timeout) < 0)
        sceKernelTerminateThread(g_probe_thread);
    sceKernelDeleteThread(g_probe_thread);
    g_probe_thread = -1;
    pw_spike_suspend_shutdown();
}
