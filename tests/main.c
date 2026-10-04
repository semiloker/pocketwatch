// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pwtest.h"

#define PWT_MAX 256

static struct {
    const char *name;
    pwt_fn fn;
} g_tests[PWT_MAX];
static int g_count;
static int g_current_failed;

void pwt_register(const char *name, pwt_fn fn)
{
    if (g_count < PWT_MAX) {
        g_tests[g_count].name = name;
        g_tests[g_count].fn = fn;
        g_count++;
    }
}

void pwt_failf(const char *file, int line, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "  %s:%d: CHECK failed: ", file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    g_current_failed = 1;
}

/* Usage: pwtest [substring]  - run all tests, or only those whose name contains substring. */
int main(int argc, char **argv)
{
    const char *filter = argc > 1 ? argv[1] : NULL;
    int run = 0, failed = 0;

    for (int i = 0; i < g_count; i++) {
        if (filter && strstr(g_tests[i].name, filter) == NULL)
            continue;
        g_current_failed = 0;
        g_tests[i].fn();
        run++;
        if (g_current_failed) {
            failed++;
            printf("FAIL %s\n", g_tests[i].name);
        } else {
            printf("ok   %s\n", g_tests[i].name);
        }
    }

    printf("\n%d test(s), %d failed\n", run, failed);
    return failed ? 1 : 0;
}
