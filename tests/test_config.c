// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/config.h"
#include "pwtest.h"

PW_TEST(config_defaults_and_parse)
{
    pw_config c;
    const char *text = "# comment = fast\r\n"
                       "fast = off\r\n"
                       "  status=0  \n"
                       "unknown = 1\n"
                       "fast = maybe\n" /* bad value: ignored */
                       "\n";

    pw_config_defaults(&c);
    CHECK_EQ(c.fast, 1);
    CHECK_EQ(c.status, 1);
    CHECK_EQ(pw_config_parse(&c, text, (unsigned int)strlen(text)), 2);
    CHECK_EQ(c.fast, 0);
    CHECK_EQ(c.status, 0);
    CHECK_EQ(pw_config_parse(&c, "fast = yes", 10), 1); /* no newline at the end */
    CHECK_EQ(c.fast, 1);
}

PW_TEST(config_format_roundtrip_and_file)
{
    pw_config c, d;
    char text[256], path[512];
    const char *tmp = getenv("PWTEST_TMPDIR");
    int n;

    pw_config_defaults(&c);
    c.fast = 0;
    n = pw_config_format(&c, text, sizeof(text));
    CHECK(n > 0);
    pw_config_defaults(&d);
    CHECK_EQ(pw_config_parse(&d, text, (unsigned int)n), 2);
    CHECK_EQ(d.fast, 0);
    CHECK_EQ(d.status, 1);
    CHECK(pw_config_format(&c, text, 10) < 0);

    snprintf(path, sizeof(path), "%s/config.txt", tmp ? tmp : ".");
    remove(path);
    CHECK_EQ(pw_config_load(path), 0); /* no file: defaults */
    CHECK_EQ(pw_config_get()->fast, 1);
    pw_config_get()->status = 0;
    CHECK_EQ(pw_config_save(path), 0);
    pw_config_get()->status = 1;
    CHECK_EQ(pw_config_load(path), 2);
    CHECK_EQ(pw_config_get()->status, 0);
    CHECK_EQ(pw_config_get()->fast, 1);
    pw_config_defaults(pw_config_get());
}
