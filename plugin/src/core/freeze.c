// SPDX-License-Identifier: GPL-3.0-or-later
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

int pw_freeze_game(pw_freeze_set *fs, unsigned int timeout_ms)
{
    static SceUID all[128];
    SceUID pending[PW_FREEZE_MAX];
    int total = 0, npending = 0;
    unsigned int t0 = sceKernelGetSystemTimeLow();

    fs->count = 0;
    fs->forced = 0;
    fs->ms = 0;
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

    fs->ms = (sceKernelGetSystemTimeLow() - t0) / 1000;
    return fs->count;
}

void pw_thaw_game(pw_freeze_set *fs)
{
    for (int i = 0; i < fs->count; i++)
        sceKernelResumeThread(fs->ids[i]);
    fs->count = 0;
}

void pw_freeze_log(const pw_freeze_set *fs)
{
    pw_log("freeze: %d game thread(s) suspended in %u ms (%d forced past the timeout)", fs->count,
           fs->ms, fs->forced);
    for (int i = 0; i < fs->count; i++) {
        SceKernelThreadInfo info;

        memset(&info, 0, sizeof(info));
        info.size = sizeof(info);
        if (sceKernelReferThreadStatus(fs->ids[i], &info) >= 0)
            pw_log("freeze:   %-31.31s prio=%3d status=0x%02X wait=%d", info.name,
                   info.currentPriority, info.status, info.waitType);
    }
}
