// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspintrman.h>
#include <pspiofilemgr.h>

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
