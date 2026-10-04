// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <pspsysclib.h>
#include <stdarg.h>
#include <string.h>

#include "log.h"

#define PW_LOG_NAME     "pocketwatch.log"
#define PW_LOG_PATH_MAX 256
#define PW_LOG_LINE_MAX 256

/* Empty path means logging is disabled. */
static char g_log_path[PW_LOG_PATH_MAX];

typedef struct {
    char *buf;
    unsigned int len;
    unsigned int cap;
} FmtBuf;

static void fmt_putc(void *ctx, int ch)
{
    FmtBuf *fb = ctx;

    /* prnt signals start/end of output with these out-of-band values. */
    if (ch == 0x200 || ch == 0x201)
        return;
    if (fb->len + 1 < fb->cap)
        fb->buf[fb->len++] = (char)ch;
}

void pw_log_init(const char *module_path)
{
    const char *slash = strrchr(module_path, '/');
    unsigned int dir_len;

    g_log_path[0] = '\0';
    if (slash == NULL)
        return;

    dir_len = (unsigned int)(slash - module_path) + 1;
    if (dir_len + sizeof(PW_LOG_NAME) > sizeof(g_log_path))
        return;

    memcpy(g_log_path, module_path, dir_len);
    strcpy(g_log_path + dir_len, PW_LOG_NAME);

    SceUID fd = sceIoOpen(g_log_path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) {
        g_log_path[0] = '\0';
        return;
    }
    sceIoClose(fd);
}

void pw_log(const char *fmt, ...)
{
    char line[PW_LOG_LINE_MAX];
    FmtBuf fb = {line, 0, sizeof(line) - 1}; /* keep one byte for '\n' */
    va_list ap;

    if (g_log_path[0] == '\0')
        return;

    va_start(ap, fmt);
    prnt(fmt_putc, &fb, fmt, ap);
    va_end(ap);
    line[fb.len++] = '\n';

    SceUID fd = sceIoOpen(g_log_path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd < 0)
        return;
    sceIoWrite(fd, line, fb.len);
    sceIoClose(fd);
}
