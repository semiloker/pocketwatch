// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>
#include <pspintrman.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <pspiofilemgr.h>
#include <psprtc.h>
#include <string.h>

#include "core/platform.h"

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

unsigned int pw_buttons_held(void)
{
    SceCtrlLatch latch;

    /* For a kernel caller this is the kernel latch: btnPress is the last sample, unmasked
       (uOFW src/kd/ctrl/ctrl.c, _sceCtrlUpdateButtons and sceCtrlPeekLatch). */
    return sceCtrlPeekLatch(&latch) < 0 ? 0 : latch.uiPress;
}
