// SPDX-License-Identifier: GPL-3.0-or-later
/* Host implementation of core/platform.h for unit tests. */
#include <stdio.h>
#include <sys/stat.h>

#include "core/platform.h"

unsigned int pw_irq_lock(void)
{
    return 0;
}

void pw_irq_unlock(unsigned int state)
{
    (void)state;
}

int pw_file_write(const char *path, const void *data, unsigned int len, int append)
{
    FILE *f = fopen(path, append ? "ab" : "wb");
    size_t written;

    if (f == NULL)
        return -1;
    written = len > 0 ? fwrite(data, 1, len, f) : 0;
    fclose(f);
    return (int)written;
}

int pw_file_size(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0)
        return -1;
    return (int)st.st_size;
}

int pw_file_read(const char *path, void *data, unsigned int cap)
{
    FILE *f = fopen(path, "rb");
    size_t got;

    if (f == NULL)
        return -1;
    got = fread(data, 1, cap, f);
    fclose(f);
    return (int)got;
}
