// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include <pspsysevent.h>
#include <string.h>

#include "core/crc32.h"
#include "core/log.h"
#include "core/mem.h"
#include "debug/spike_suspend.h"

#define KERNEL_RAM_BASE 0x88000000u
#define KERNEL_RAM_SIZE 0x00400000u /* P1 + P4, see docs/research/m1-hardware.md */

#define EV_POWER_LOCK_PHASE1 0x00000401
#define EV_SUSPEND_PHASE0_0  0x00004000
#define EV_RESUME_COMPLETED  0x00400000

#define MAX_EVENTS     96
#define WAKE_DELAY_US  100000ull
#define RESUME_TIMEOUT 2000 /* polls of 5 ms of *active* time; the sleep itself does not count */

int sceRtcSetAlarmTick(u64 *tick); /* sceRtc_driver, not declared by PSPSDK */

/* Everything the handler touches: plain memory only, no I/O, no logging. */
static volatile int g_armed;
static volatile int g_alarm_set;
static volatile int g_captured;
static volatile int g_resumed;
static volatile unsigned int g_capture_cycles;
static volatile unsigned int g_dmac_left;
static volatile unsigned int g_events[MAX_EVENTS];
static volatile int g_event_count;
static void *volatile g_stage;

static int g_registered;

static unsigned int cycles(void)
{
    unsigned int c;

    __asm__ volatile("mfc0 %0, $9" : "=r"(c)); /* CP0 Count */
    return c;
}

/* Wait until the DMA controllers are idle so the copy sees a settled kernel image. */
static unsigned int dmac_drain(void)
{
    volatile unsigned int *en0 = (volatile unsigned int *)(0xBC900000 + 0x1C);
    volatile unsigned int *en1 = (volatile unsigned int *)(0xBCA00000 + 0x1C);
    unsigned int spin = 0;

    while ((*en0 | *en1) && spin < 200000)
        spin++;
    return *en0 | *en1;
}

static int on_sysevent(int ev_id, char *ev_name, void *param, int *result)
{
    (void)ev_name;
    (void)param;
    (void)result;

    if (!g_armed)
        return 0;
    if (g_event_count < MAX_EVENTS)
        g_events[g_event_count++] = (unsigned int)ev_id;

    if (ev_id == EV_POWER_LOCK_PHASE1 && !g_alarm_set) {
        u64 tick;

        sceRtcGetCurrentTick(&tick);
        tick += WAKE_DELAY_US; /* RTC ticks are microseconds */
        sceRtcSetAlarmTick(&tick);
        g_alarm_set = 1;
    } else if (ev_id == EV_SUSPEND_PHASE0_0 && !g_captured && g_stage) {
        unsigned int t0 = cycles();

        g_dmac_left = dmac_drain();
        memcpy(g_stage, (const void *)KERNEL_RAM_BASE, KERNEL_RAM_SIZE);
        sceKernelDcacheWritebackAll(); /* the copy must reach DRAM before power-down */
        g_capture_cycles = cycles() - t0;
        g_captured = 1;
    } else if (ev_id == EV_RESUME_COMPLETED) {
        sceRtcSetAlarmTick(NULL); /* do not wake the next natural sleep early */
        g_resumed = 1;
    }
    return 0;
}

static PspSysEventHandler g_handler = {
    sizeof(PspSysEventHandler), "PocketwatchE1", 0x00FFFF00, on_sysevent, 0, 0, NULL, {0},
};

int pw_spike_suspend_init(void)
{
    int ret = sceKernelRegisterSysEventHandler(&g_handler);

    g_registered = ret >= 0;
    return ret;
}

void pw_spike_suspend_shutdown(void)
{
    if (g_registered)
        sceKernelUnregisterSysEventHandler(&g_handler);
    g_registered = 0;
}

static void log_events(void)
{
    char line[200];
    int n = 0, len = 0;

    pw_log("E1: %d sysevent(s):", g_event_count);
    for (int i = 0; i < g_event_count; i++) {
        static const char digits[] = "0123456789ABCDEF";
        unsigned int v = g_events[i];

        line[len++] = ' ';
        for (int s = 28; s >= 0; s -= 4)
            line[len++] = digits[(v >> s) & 15];
        if (++n == 8 || i == g_event_count - 1) {
            line[len] = '\0';
            pw_log("E1:  %s", line);
            n = len = 0;
        }
    }
}

void pw_spike_suspend(void)
{
    pw_mem_block stage;
    u64 t_start = 0, t_end = 0;
    int ret, polls = 0;

    pw_log("E1: ---- start ----");
    if (!g_registered) {
        pw_log("E1: sysevent handler not registered");
        return;
    }
    ret = pw_mem_alloc(&stage, "PwE1Stage", KERNEL_RAM_SIZE);
    if (ret < 0) {
        pw_log("E1: staging alloc failed 0x%08X", ret);
        return;
    }
    memset(stage.ptr, 0, KERNEL_RAM_SIZE);
    pw_log("E1: staging %u KB at %p", KERNEL_RAM_SIZE / 1024, stage.ptr);

    g_event_count = 0;
    g_alarm_set = 0;
    g_captured = 0;
    g_resumed = 0;
    g_capture_cycles = 0;
    g_stage = stage.ptr;
    sceKernelDcacheWritebackAll();
    g_armed = 1;

    sceRtcGetCurrentTick(&t_start);
    ret = scePowerRequestSuspend();
    while (!g_resumed && polls++ < RESUME_TIMEOUT)
        sceKernelDelayThread(5000);
    sceRtcGetCurrentTick(&t_end);
    g_armed = 0;
    g_stage = NULL;

    pw_log("E1: scePowerRequestSuspend=0x%08X resumed=%d alarm=%d captured=%d", ret, g_resumed,
           g_alarm_set, g_captured);
    pw_log("E1: request->resume %u ms (wall clock), capture %u cycles, dmac_left=0x%X",
           (unsigned int)((t_end - t_start) / 1000), g_capture_cycles, g_dmac_left);
    log_events();

    if (g_captured) {
        /* sysmem's code (first 64 KB) never changes: the copy must match it exactly. */
        int text_ok = memcmp(stage.ptr, (const void *)KERNEL_RAM_BASE, 0x10000) == 0;

        pw_log("E1: snapshot crc32=%08X, sysmem text %s", pw_crc32(0, stage.ptr, KERNEL_RAM_SIZE),
               text_ok ? "matches live memory" : "DIFFERS from live memory");
    }
    if (!g_resumed)
        sceRtcSetAlarmTick(NULL);

    pw_mem_free(&stage);
    pw_log("E1: ---- end ----");
}
