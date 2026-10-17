// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdarg.h>

#include "core/platform.h"
#include "stb_sprintf.h"
#include "xlog.h"
#include "xsettings.h"

#define LOG_CAP 2048

static char g_buf[LOG_CAP];
static int g_len, g_written;

void pwx_log(const char *fmt, ...)
{
    va_list ap;
    int n;

    if (g_len > LOG_CAP - 2)
        return;
    va_start(ap, fmt);
    n = stbsp_vsnprintf(g_buf + g_len, LOG_CAP - 1 - g_len, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    g_len += n < LOG_CAP - 1 - g_len ? n : LOG_CAP - 2 - g_len;
    g_buf[g_len++] = '\n';
}

void pwx_log_flush(void)
{
    if (g_len == 0)
        return;
    pw_file_write(PWX_DIR "xmb.log", g_buf, (unsigned int)g_len, g_written);
    g_written = 1;
    g_len = 0;
}
