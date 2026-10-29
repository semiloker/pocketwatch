// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * What the shared portable code (core/config.c) needs from the system, for Home (user mode,
 * newlib): the files of core/platform.h.
 */
#include <pspiofilemgr.h>

#include "core/platform.h"

int pw_file_write(const char *path, const void *data, unsigned int len, int append)
{
    int flags = PSP_O_WRONLY | PSP_O_CREAT | (append ? PSP_O_APPEND : PSP_O_TRUNC);
    SceUID fd = sceIoOpen(path, flags, 0777);
    int written;

    if (fd < 0)
        return fd;
    written = len > 0 ? sceIoWrite(fd, data, len) : 0;
    sceIoClose(fd);
    return written;
}

int pw_file_read(const char *path, void *data, unsigned int cap)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    int n;

    if (fd < 0)
        return fd;
    n = sceIoRead(fd, data, cap);
    sceIoClose(fd);
    return n;
}

int pw_file_remove(const char *path)
{
    return sceIoRemove(path);
}
