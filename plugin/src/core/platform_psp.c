// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspintrman.h>
#include <pspiofilemgr.h>
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
