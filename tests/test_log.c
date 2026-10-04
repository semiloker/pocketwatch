// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <stdlib.h>

#include "core/log.h"
#include "pwtest.h"

/* Builds "<tmpdir>/pocketwatch.prx" so the log lands in <tmpdir>/pocketwatch.log. */
static const char *fake_module_path(char *log_path, size_t log_len)
{
    static char module[512];
    const char *dir = getenv("PWTEST_TMPDIR");

    if (dir == NULL)
        dir = ".";
    snprintf(module, sizeof(module), "%s/pocketwatch.prx", dir);
    snprintf(log_path, log_len, "%s/pocketwatch.log", dir);
    return module;
}

static size_t read_file(const char *path, char *out, size_t cap)
{
    FILE *f = fopen(path, "rb");
    size_t n;

    if (f == NULL)
        return 0;
    n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    return n;
}

PW_TEST(log_writes_formatted_lines)
{
    char log_path[512], content[1024];

    pw_log_init(fake_module_path(log_path, sizeof(log_path)));
    pw_log("Pocketwatch %s loaded", "v0.0.0");
    pw_log("slot=%d size=%u hex=%08X", 2, 15u * 1024 * 1024, 0xDEADBEEFu);
    read_file(log_path, content, sizeof(content));
    CHECK_STR(content, "Pocketwatch v0.0.0 loaded\nslot=2 size=15728640 hex=DEADBEEF\n");
}

PW_TEST(log_init_truncates_previous_boot)
{
    char log_path[512], content[256];

    pw_log_init(fake_module_path(log_path, sizeof(log_path)));
    pw_log("old boot");
    pw_log_init(fake_module_path(log_path, sizeof(log_path)));
    pw_log("new boot");
    read_file(log_path, content, sizeof(content));
    CHECK_STR(content, "new boot\n");
}

PW_TEST(log_deferred_keeps_lines_in_ram_until_flush)
{
    char log_path[512], content[256];

    pw_log_init(fake_module_path(log_path, sizeof(log_path)));
    pw_log_set_deferred(1);
    pw_log("inside sysevent");
    read_file(log_path, content, sizeof(content));
    CHECK_STR(content, "");

    pw_log_set_deferred(0);
    pw_log_flush();
    read_file(log_path, content, sizeof(content));
    CHECK_STR(content, "inside sysevent\n");
}

PW_TEST(log_reports_dropped_lines_when_ring_overflows)
{
    char log_path[512];
    static char content[16384];
    char line[200];

    pw_log_init(fake_module_path(log_path, sizeof(log_path)));
    pw_log_set_deferred(1);
    memset(line, 'x', sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';
    for (int i = 0; i < 60; i++) /* 60 * 200 bytes > 8 KB ring */
        pw_log("%s", line);
    pw_log_set_deferred(0);
    pw_log_flush();

    read_file(log_path, content, sizeof(content));
    CHECK(strstr(content, "[log] ") != NULL);
    CHECK(strstr(content, "line(s) dropped") != NULL);
}

PW_TEST(log_truncates_overlong_line)
{
    char log_path[512], content[1024], big[600];

    pw_log_init(fake_module_path(log_path, sizeof(log_path)));
    memset(big, 'y', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    pw_log("%s", big);
    CHECK_EQ(read_file(log_path, content, sizeof(content)), 255); /* 254 chars + '\n' */
    CHECK_EQ(content[254], '\n');
}

PW_TEST(log_disabled_without_directory_in_path)
{
    char content[64];

    pw_log_init("no-slash-here");
    pw_log("should go nowhere");
    pw_log_flush();
    CHECK_EQ(read_file("pocketwatch.log", content, sizeof(content)), 0);
}
