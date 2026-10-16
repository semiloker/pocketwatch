// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdarg.h>
#include <string.h>

#include "stb_sprintf.h"

#include "core/log.h"
#include "core/platform.h"
#include "core/ring.h"

#define PW_LOG_NAME         "pocketwatch.log"
#define PW_LOG_PATH_MAX     256
#define PW_LOG_LINE_MAX     256
#define PW_LOG_RING_SIZE    16384 /* holds a few loads worth of lines while a state is held */
#define PW_LOG_CHUNK        512
#define PW_LOG_MAX_BYTES    (256 * 1024)
#define PW_LOG_SESSION_MARK "\n==== new session ====\n"

/* Empty path means logging is disabled. */
static char g_log_path[PW_LOG_PATH_MAX];

static char g_ring_buf[PW_LOG_RING_SIZE];
static pw_ring g_ring;
static int g_deferred;
static int g_blocked; /* the card must not be written at all until the next boot */
static int g_flushing;
static char g_chunk[PW_LOG_CHUNK]; /* only touched by the thread that owns g_flushing */

void pw_log_init(const char *module_path)
{
    const char *slash = strrchr(module_path, '/');
    unsigned int dir_len;

    pw_ring_init(&g_ring, g_ring_buf, sizeof(g_ring_buf));
    g_deferred = 0;
    g_blocked = 0;
    g_flushing = 0;
    g_log_path[0] = '\0';
    if (slash == NULL)
        return;

    dir_len = (unsigned int)(slash - module_path) + 1;
    if (dir_len + sizeof(PW_LOG_NAME) > sizeof(g_log_path))
        return;

    memcpy(g_log_path, module_path, dir_len);
    strcpy(g_log_path + dir_len, PW_LOG_NAME);

    /*
     * Keep earlier sessions: when a game crashes, the evidence is in the previous session's
     * lines, and the user restarts a game before the card is read. Start over only past the cap.
     */
    int size = pw_file_size(g_log_path);
    int ret;

    if (size < 0 || size > PW_LOG_MAX_BYTES)
        ret = pw_file_write(g_log_path, "", 0, 0);
    else if (size > 0)
        ret = pw_file_write(g_log_path, PW_LOG_SESSION_MARK, sizeof(PW_LOG_SESSION_MARK) - 1, 1);
    else
        ret = 0;
    if (ret < 0)
        g_log_path[0] = '\0';
}

void pw_log(const char *fmt, ...)
{
    char line[PW_LOG_LINE_MAX];
    unsigned int state;
    va_list ap;
    int len;

    if (g_log_path[0] == '\0')
        return;

    va_start(ap, fmt);
    len = stbsp_vsnprintf(line, sizeof(line) - 1, fmt, ap); /* keep one byte for '\n' */
    va_end(ap);
    if (len < 0)
        return;
    if (len > (int)sizeof(line) - 2)
        len = (int)sizeof(line) - 2;
    line[len++] = '\n';

    state = pw_irq_lock();
    pw_ring_push(&g_ring, line, (unsigned int)len);
    pw_irq_unlock(state);

    if (!g_deferred)
        pw_log_flush();
}

void pw_log_set_deferred(int deferred)
{
    g_deferred = deferred;
}

void pw_log_set_card_blocked(int blocked)
{
    g_blocked = blocked;
}

int pw_log_card_blocked(void)
{
    return g_blocked;
}

int pw_log_dir(char *out, unsigned int cap)
{
    const char *slash = strrchr(g_log_path, '/');
    unsigned int len;

    if (slash == NULL)
        return -1;
    len = (unsigned int)(slash - g_log_path) + 1;
    if (len + 1 > cap)
        return -1;
    memcpy(out, g_log_path, len);
    out[len] = '\0';
    return (int)len;
}

void pw_log_flush(void)
{
    unsigned int state, n, dropped;
    int pending;

    if (g_log_path[0] == '\0' || g_blocked)
        return;

    state = pw_irq_lock();
    if (g_flushing) {
        /* The thread already flushing will drain whatever we just added. */
        pw_irq_unlock(state);
        return;
    }
    g_flushing = 1;
    pw_irq_unlock(state);

    do {
        for (;;) {
            state = pw_irq_lock();
            dropped = g_ring.dropped;
            g_ring.dropped = 0;
            n = pw_ring_pop(&g_ring, g_chunk, sizeof(g_chunk));
            pw_irq_unlock(state);

            if (dropped > 0) {
                char note[48];
                int note_len =
                    stbsp_snprintf(note, sizeof(note), "[log] %u line(s) dropped\n", dropped);
                pw_file_write(g_log_path, note, (unsigned int)note_len, 1);
            }
            if (n == 0)
                break;
            pw_file_write(g_log_path, g_chunk, n, 1);
        }

        /* Lines pushed after our last pop but before we release ownership must not be stranded. */
        state = pw_irq_lock();
        pending = g_ring.len > 0 && !g_deferred && !g_blocked;
        if (!pending)
            g_flushing = 0;
        pw_irq_unlock(state);
    } while (pending);
}
