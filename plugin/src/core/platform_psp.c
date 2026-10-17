// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>
#include <pspintrman.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <pspiofilemgr.h>
#include <psprtc.h>
#include <string.h>

#include "core/log.h"
#include "core/platform.h"
#include "core/sysctrl.h"

unsigned int pw_irq_lock(void)
{
    return sceKernelCpuSuspendIntr();
}

void pw_irq_unlock(unsigned int state)
{
    sceKernelCpuResumeIntr(state);
}

int pw_file_write(const char *path, const void *data, unsigned int len, int append)
{
    int flags = PSP_O_WRONLY | PSP_O_CREAT | (append ? PSP_O_APPEND : PSP_O_TRUNC);
    SceUID fd = sceIoOpen(path, flags, 0777);
    int written = 0;

    if (fd < 0)
        return fd;
    if (len > 0)
        written = sceIoWrite(fd, data, len);
    sceIoClose(fd);
    return written;
}

int pw_file_read(const char *path, void *data, unsigned int cap)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    int got;

    if (fd < 0)
        return fd;
    got = sceIoRead(fd, data, cap);
    sceIoClose(fd);
    return got;
}

int pw_file_size(const char *path)
{
    SceIoStat st;
    int ret;

    memset(&st, 0, sizeof(st));
    ret = sceIoGetstat(path, &st);
    if (ret < 0)
        return ret;
    return st.st_size > 0x7FFFFFFF ? 0x7FFFFFFF : (int)st.st_size;
}

int pw_file_open(const char *path, int write)
{
    return sceIoOpen(path, write ? PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC : PSP_O_RDONLY, 0777);
}

int pw_file_put(int fd, const void *data, unsigned int len)
{
    return sceIoWrite(fd, data, len);
}

int pw_file_get(int fd, void *data, unsigned int len)
{
    return sceIoRead(fd, data, len);
}

void pw_file_close(int fd)
{
    sceIoClose(fd);
}

int pw_file_remove(const char *path)
{
    return sceIoRemove(path);
}

int pw_dir_make(const char *path)
{
    return sceIoMkdir(path, 0777);
}

int pw_dir_remove(const char *path)
{
    return sceIoRmdir(path);
}

_Static_assert(sizeof(pw_datetime) == sizeof(ScePspDateTime), "pw_datetime is ScePspDateTime");

void pw_local_time(pw_datetime *t)
{
    memset(t, 0, sizeof(*t));
    sceRtcGetCurrentClockLocalTime((ScePspDateTime *)t);
}

void pw_delay_ms(unsigned int ms)
{
    sceKernelDelayThread(ms * 1000);
}

unsigned int pw_time_ms(void)
{
    return sceKernelGetSystemTimeLow() / 1000;
}

/*
 * sceCtrlPeekBufferPositive2(port, data, count): the same kernel buffer as
 * sceCtrlPeekBufferPositive, for port 0 (the PSP's own buttons) in the 48-byte SceCtrlData2 layout
 * (uOFW src/kd/ctrl/ctrl.c, _sceCtrlReadBuf; include/ctrl.h). ARK-4 does not hook it. Found at
 * run time, so a missing export costs only ARK's combos guard: sceCtrl's NID (user libraries keep
 * their NIDs) first, then sceCtrl_driver's 6.60 NID (uOFW exports.exp; ARK-4 uses its 6.60 table
 * on 6.61).
 */
#define NID_PEEK2_USER   0x5A36B1C2
#define NID_PEEK2_DRIVER 0xD4692E77

typedef struct {
    unsigned int stamp, buttons;
    unsigned char ax, ay, rx, ry, rsrv[4];
    int sense[8];
} ctrl_data2;

static int (*g_peek2)(unsigned int port, ctrl_data2 *data, unsigned char count);
static int g_peek2_looked;

unsigned int pw_buttons_held(void)
{
    SceCtrlData pad;

    if (!g_peek2_looked) {
        g_peek2_looked = 1;
        g_peek2 = (void *)sctrlHENFindFunction("sceController_Service", "sceCtrl", NID_PEEK2_USER);
        if (!g_peek2)
            g_peek2 = (void *)sctrlHENFindFunction("sceController_Service", "sceCtrl_driver",
                                                   NID_PEEK2_DRIVER);
        if (g_peek2)
            pw_log("ctrl: buttons through sceCtrlPeekBufferPositive2 at %08X",
                   (unsigned int)g_peek2);
        else
            pw_log(
                "ctrl: no sceCtrlPeekBufferPositive2: ARK's exit combos stay live in our threads");
    }
    if (g_peek2) {
        ctrl_data2 data;

        return g_peek2(0, &data, 1) < 0 ? 0 : data.buttons;
    }
    return sceCtrlPeekBufferPositive(&pad, 1) < 0 ? 0 : pad.Buttons;
}
