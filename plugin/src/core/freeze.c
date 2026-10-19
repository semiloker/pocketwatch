// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <pspthreadman_kernel.h>
#include <string.h>

#include "core/freeze.h"
#include "core/log.h"

#define ATTR_USER        0x80000000u
#define STATUS_WAITING   0x04
#define STATUS_SUSPENDED 0x08
#define STATUS_DORMANT   0x10
#define STATUS_DEAD      0x20
#define POLL_US          1000
#define CARD_PROBE_MS    50   /* a card nobody holds answers a stat at once */
#define CARD_GIVE_UP_MS  2000 /* the game kept the card this long: no freeze */
#define CARD_RELEASE_MS  500  /* the game runs at most this long to let the card go */

/* A stat of the card on a thread of its own: it answers once nobody holds the FAT driver. One
   probe at a time; one that has not answered yet is waited for, not replaced. */
static SceUID g_probe = -1;
static volatile int g_probe_out;

static int thread_status(SceUID id, unsigned int *attr)
{
    SceKernelThreadInfo info;

    memset(&info, 0, sizeof(info));
    info.size = sizeof(info);
    if (sceKernelReferThreadStatus(id, &info) < 0)
        return -1;
    if (attr)
        *attr = info.attr;
    return info.status;
}

/* Suspend id if it is (still) WAITING, atomically with respect to the scheduler. */
static int suspend_if_waiting(SceUID id, int force)
{
    int ok = 0;
    int ds = sceKernelSuspendDispatchThread();
    int status = thread_status(id, NULL);

    if (status >= 0 && (force || status == STATUS_WAITING) &&
        !(status & (STATUS_SUSPENDED | STATUS_DORMANT | STATUS_DEAD)))
        ok = sceKernelSuspendThread(id) >= 0;
    sceKernelResumeDispatchThread(ds);
    return ok;
}

static int probe_thread(SceSize args, void *argp)
{
    SceIoStat st;

    (void)args;
    (void)argp;
    sceIoGetstat("ms0:/PSP", &st);
    g_probe_out = 0;
    return 0;
}

/* 1 when the probe answered within ms (a probe sent earlier counts). */
static int card_answers(unsigned int ms)
{
    unsigned int t0 = sceKernelGetSystemTimeLow();

    if (!g_probe_out) {
        if (g_probe >= 0) { /* the last one has answered: it is returning */
            SceUInt end = 10 * 1000;

            sceKernelWaitThreadEnd(g_probe, &end);
            sceKernelDeleteThread(g_probe);
        }
        g_probe = sceKernelCreateThread("PocketwatchCardProbe", probe_thread, 0x10, 0x1000, 0, 0);
        if (g_probe < 0)
            return 1; /* no way to tell: freeze as before */
        g_probe_out = 1;
        if (sceKernelStartThread(g_probe, 0, NULL) < 0) {
            g_probe_out = 0;
            return 1;
        }
    }
    while (g_probe_out && sceKernelGetSystemTimeLow() - t0 < ms * 1000)
        sceKernelDelayThread(POLL_US);
    return !g_probe_out;
}

static int freeze_once(pw_freeze_set *fs, unsigned int timeout_ms)
{
    static SceUID all[128];
    SceUID pending[PW_FREEZE_MAX];
    int total = 0, npending = 0;
    unsigned int t0 = sceKernelGetSystemTimeLow();

    fs->count = 0;
    fs->forced = 0;
    if (sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, all, 128, &total) < 0)
        return -1;
    if (total > 128)
        total = 128;

    for (int i = 0; i < total && npending < PW_FREEZE_MAX; i++) {
        unsigned int attr = 0;
        int status = thread_status(all[i], &attr);

        if (status < 0 || !(attr & ATTR_USER))
            continue;
        if (status & (STATUS_SUSPENDED | STATUS_DORMANT | STATUS_DEAD))
            continue; /* not running, or someone else owns its suspension */
        pending[npending++] = all[i];
    }

    while (npending > 0) {
        int force = sceKernelGetSystemTimeLow() - t0 >= timeout_ms * 1000;

        for (int i = 0; i < npending;) {
            if (suspend_if_waiting(pending[i], force)) {
                fs->ids[fs->count++] = pending[i];
                fs->forced += force;
                pending[i] = pending[--npending];
            } else if (thread_status(pending[i], NULL) < 0) {
                pending[i] = pending[--npending]; /* thread exited meanwhile */
            } else {
                i++;
            }
        }
        if (npending > 0)
            sceKernelDelayThread(POLL_US);
    }
    return fs->count;
}

/*
 * A thread WAITING inside a read of the card (a plain sceIoRead, Inferno reading the ISO) holds
 * the FAT driver while it waits for the transfer. Suspended there, it keeps it: the next card
 * access of ours - a log line, the state list, a save - would wait for ever, the game frozen and
 * the menu never drawn (Need for Speed: Most Wanted, 2026-10-19). So after every freeze the card
 * must answer; when it does not, the game runs again until it lets the card go, and is frozen
 * anew.
 */
int pw_freeze_game(pw_freeze_set *fs, unsigned int timeout_ms)
{
    unsigned int t0 = sceKernelGetSystemTimeLow();
    int n;

    fs->card_tries = 0;
    fs->card_busy = 0;
    for (;;) {
        n = freeze_once(fs, timeout_ms);
        if (n <= 0 || card_answers(CARD_PROBE_MS))
            break;
        pw_thaw_game(fs);
        fs->card_tries++;
        if (sceKernelGetSystemTimeLow() - t0 >= CARD_GIVE_UP_MS * 1000) {
            fs->card_busy = 1;
            n = 0;
            break;
        }
        card_answers(CARD_RELEASE_MS); /* the game finishes its read */
    }
    fs->ms = (sceKernelGetSystemTimeLow() - t0) / 1000;
    return n;
}

void pw_thaw_game(pw_freeze_set *fs)
{
    for (int i = 0; i < fs->count; i++)
        sceKernelResumeThread(fs->ids[i]);
    fs->count = 0;
}

void pw_freeze_log(const pw_freeze_set *fs)
{
    pw_log("freeze: %d game thread(s) suspended in %u ms (%d forced past the timeout; the card "
           "was held %d time(s)%s)",
           fs->count, fs->ms, fs->forced, fs->card_tries, fs->card_busy ? ", gave up" : "");
    for (int i = 0; i < fs->count; i++) {
        SceKernelThreadInfo info;

        memset(&info, 0, sizeof(info));
        info.size = sizeof(info);
        if (sceKernelReferThreadStatus(fs->ids[i], &info) >= 0)
            pw_log("freeze:   %-31.31s prio=%3d status=0x%02X wait=%d", info.name,
                   info.currentPriority, info.status, info.waitType);
    }
}
