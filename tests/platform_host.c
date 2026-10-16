// SPDX-License-Identifier: GPL-3.0-or-later
/* Host implementation of core/platform.h for unit tests. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "core/platform.h"

/* "ms0:/..." lives under $PWTEST_TMPDIR/ms0/ so the plugin's real paths work in tests. */
static const char *host_path(const char *path)
{
    static char buf[4][512];
    static int next;
    const char *tmp = getenv("PWTEST_TMPDIR");
    char *out;

    if (strncmp(path, "ms0:/", 5) != 0)
        return path;
    out = buf[next++ & 3];
    snprintf(out, sizeof(buf[0]), "%s/ms0/%s", tmp ? tmp : ".", path + 5);
    return out;
}

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
    FILE *f = fopen(host_path(path), append ? "ab" : "wb");
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

    if (stat(host_path(path), &st) != 0)
        return errno == ENOENT ? (int)0x80010002 : -1; /* the PSP's "file not found" */
    return (int)st.st_size;
}

int pw_file_read(const char *path, void *data, unsigned int cap)
{
    FILE *f = fopen(host_path(path), "rb");
    size_t got;

    if (f == NULL)
        return -1;
    got = fread(data, 1, cap, f);
    fclose(f);
    return (int)got;
}

#define HOST_FILES 8
static FILE *g_files[HOST_FILES];

int pw_file_open(const char *path, int write)
{
    for (int i = 0; i < HOST_FILES; i++)
        if (g_files[i] == NULL) {
            g_files[i] = fopen(host_path(path), write ? "wb" : "rb");
            return g_files[i] ? i : -1;
        }
    return -1;
}

int pw_file_put(int fd, const void *data, unsigned int len)
{
    return (int)fwrite(data, 1, len, g_files[fd]);
}

int pw_file_get(int fd, void *data, unsigned int len)
{
    return (int)fread(data, 1, len, g_files[fd]);
}

void pw_file_close(int fd)
{
    fclose(g_files[fd]);
    g_files[fd] = NULL;
}

int pw_file_remove(const char *path)
{
    return unlink(host_path(path));
}

int pw_dir_make(const char *path)
{
    return mkdir(host_path(path), 0777);
}

int pw_dir_remove(const char *path)
{
    return rmdir(host_path(path));
}

void pw_local_time(pw_datetime *t)
{
    time_t now = time(NULL);
    struct tm tm;

    localtime_r(&now, &tm);
    memset(t, 0, sizeof(*t));
    t->year = (unsigned short)(tm.tm_year + 1900);
    t->month = (unsigned short)(tm.tm_mon + 1);
    t->day = (unsigned short)tm.tm_mday;
    t->hour = (unsigned short)tm.tm_hour;
    t->minute = (unsigned short)tm.tm_min;
    t->second = (unsigned short)tm.tm_sec;
}

void pw_delay_ms(unsigned int ms)
{
    usleep(ms * 1000);
}

unsigned int pw_time_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
