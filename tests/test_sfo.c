// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>

#include "complications/states/sfo.h"
#include "pwtest.h"

/* Same inputs as tests/fixtures/make_fixtures.py. */
static int build_basic(unsigned char *out, unsigned int cap, unsigned int *params)
{
    static unsigned char file_list[PW_SFO_FILE_LIST_SIZE];
    pw_savedata_sfo s = {
        .title = "Need for Speed\xE2\x84\xA2 Most Wanted 5-1-0",
        .savedata_title = "Save State 2",
        .detail = "Slot 2\n2026-10-04 13:22",
        .directory = "PWULUS1003602",
        .file_list = file_list,
        .parental_level = 1,
    };

    memset(file_list, 0, sizeof(file_list));
    pw_sfo_file_list_entry(file_list, "STATE.BIN");
    return pw_sfo_build_savedata(&s, out, cap, params);
}

static size_t read_fixture(const char *name, unsigned char *out, size_t cap)
{
    char path[256];
    FILE *f;
    size_t n;

    snprintf(path, sizeof(path), "fixtures/%s", name);
    f = fopen(path, "rb");
    if (f == NULL)
        return 0;
    n = fread(out, 1, cap, f);
    fclose(f);
    return n;
}

PW_TEST(sfo_savedata_matches_python_reference_byte_for_byte)
{
    static unsigned char ours[8192], ref[8192];
    unsigned int params = 0;
    int size = build_basic(ours, sizeof(ours), &params);
    size_t ref_size = read_fixture("savedata_basic.sfo", ref, sizeof(ref));

    CHECK_EQ(size, PW_SFO_SAVEDATA_SIZE);
    CHECK_EQ(ref_size, PW_SFO_SAVEDATA_SIZE);
    CHECK(memcmp(ours, ref, PW_SFO_SAVEDATA_SIZE) == 0);
}

PW_TEST(sfo_params_offset_matches_find)
{
    static unsigned char sfo[PW_SFO_SAVEDATA_SIZE];
    unsigned int params = 0, off = 0, len = 0;

    CHECK_EQ(build_basic(sfo, sizeof(sfo), &params), PW_SFO_SAVEDATA_SIZE);
    CHECK_EQ(pw_sfo_find(sfo, sizeof(sfo), "SAVEDATA_PARAMS", &off, &len), 0);
    CHECK_EQ(off, params);
    CHECK_EQ(len, PW_SFO_PARAMS_SIZE);
    CHECK_EQ(params, 0x11B0); /* the "11C0"/"11D0" hashes sit at params + 0x10 / + 0x20 */
}

PW_TEST(sfo_find_returns_values)
{
    static unsigned char sfo[PW_SFO_SAVEDATA_SIZE];
    unsigned int params, off = 0, len = 0;

    build_basic(sfo, sizeof(sfo), &params);
    CHECK_EQ(pw_sfo_find(sfo, sizeof(sfo), "SAVEDATA_DIRECTORY", &off, &len), 0);
    CHECK_EQ(len, 14);
    CHECK_STR((const char *)sfo + off, "PWULUS1003602");
    CHECK_EQ(pw_sfo_find(sfo, sizeof(sfo), "NO_SUCH_KEY", &off, &len), -1);
    CHECK_EQ(pw_sfo_find(sfo, sizeof(sfo), "TITLE", &off, &len), 0); /* not "SAVEDATA_TITLE" */
    CHECK_STR((const char *)sfo + off, "Need for Speed\xE2\x84\xA2 Most Wanted 5-1-0");
}

PW_TEST(sfo_find_rejects_garbage)
{
    unsigned char junk[64];
    unsigned int off, len;

    memset(junk, 0xFF, sizeof(junk));
    CHECK_EQ(pw_sfo_find(junk, sizeof(junk), "TITLE", &off, &len), -1);
    memcpy(junk, "\0PSF", 4); /* valid magic, absurd entry count */
    CHECK_EQ(pw_sfo_find(junk, sizeof(junk), "TITLE", &off, &len), -1);
}

PW_TEST(sfo_rejects_too_long_strings_and_small_buffers)
{
    static unsigned char sfo[PW_SFO_SAVEDATA_SIZE];
    static char detail[1100];
    unsigned int params;
    pw_savedata_sfo s = {"t", "s", detail, "PWX", NULL, 1};

    memset(detail, 'x', sizeof(detail) - 1);
    detail[sizeof(detail) - 1] = '\0';
    CHECK_EQ(pw_sfo_build_savedata(&s, sfo, sizeof(sfo), &params), -1); /* > 1024 bytes */

    s.detail = "ok";
    CHECK_EQ(pw_sfo_build_savedata(&s, sfo, sizeof(sfo) - 1, &params), -1);
    CHECK_EQ(pw_sfo_build_savedata(&s, sfo, sizeof(sfo), &params), PW_SFO_SAVEDATA_SIZE);
}

PW_TEST(sfo_file_list_entry_layout)
{
    unsigned char e[PW_SFO_FILE_ENTRY_SIZE];

    CHECK_EQ(pw_sfo_file_list_entry(e, "STATE.BIN"), 0);
    CHECK(memcmp(e, "STATE.BIN\0\0\0\0", 13) == 0);
    for (int i = 13; i < PW_SFO_FILE_ENTRY_SIZE; i++)
        CHECK_EQ(e[i], 0);
    CHECK_EQ(pw_sfo_file_list_entry(e, "THIRTEEN.CHAR"), -1);
}
