// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <string.h>

#include "complications/states/engine.h"
#include "core/log.h"
#include "core/mem.h"
#include "debug/cardtest.h"
#include "stb_sprintf.h"

#define T1_ARMED      0x41315054u  /* "TP1A" */
#define PROBE_BYTES   (128 * 1024) /* several clusters on any card */
#define CHUNK         (16 * 1024)
#define NOT_FOUND     0x80010002u
#define READY_WAIT_MS 3000

enum { U_MAGIC, U_SEED, U_BYTES, U_COUNT };

/* Outcome of the hook, written in the restored world (not rolled back again). */
static volatile int g_ran, g_found, g_size_ok, g_data_ok, g_b_written, g_a_still_ok, g_b_ok;
static volatile int g_stat_ret, g_wait_ms;

static void probe_path(char *out, unsigned int cap, char which, unsigned int seed)
{
    char dir[200];

    if (pw_log_dir(dir, sizeof(dir)) < 0)
        strcpy(dir, "ms0:/seplugins/pocketwatch/");
    stbsp_snprintf(out, (int)cap, "%st1%c-%08X.bin", dir, which, seed);
}

static void fill(unsigned int *buf, unsigned int words, unsigned int seed, unsigned int first)
{
    for (unsigned int i = 0; i < words; i++)
        buf[i] = seed ^ ((first + i) * 0x9E3779B1u);
}

static int write_probe(const char *path, unsigned int seed, unsigned int *buf)
{
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    int ok = fd >= 0;

    for (unsigned int off = 0; ok && off < PROBE_BYTES; off += CHUNK) {
        fill(buf, CHUNK / 4, seed, off / 4);
        ok = sceIoWrite(fd, buf, CHUNK) == CHUNK;
    }
    if (fd >= 0)
        sceIoClose(fd);
    return ok ? 0 : -1;
}

/* buf holds two chunks: what is read and what it must be. */
static int verify_probe(const char *path, unsigned int seed, unsigned int *buf)
{
    unsigned int *want = buf + CHUNK / 4;
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    int ok = fd >= 0;

    for (unsigned int off = 0; ok && off < PROBE_BYTES; off += CHUNK) {
        fill(want, CHUNK / 4, seed, off / 4);
        ok = sceIoRead(fd, buf, CHUNK) == CHUNK && memcmp(buf, want, CHUNK) == 0;
    }
    if (fd >= 0)
        sceIoClose(fd);
    return ok ? 0 : -1;
}

/* After the rollback, game frozen, before the engine writes anything to the card. */
static int check(unsigned int *user, unsigned int words)
{
    char a[256], b[256];
    pw_mem_block mem;
    SceIoStat st;
    unsigned int t0 = sceKernelGetSystemTimeLow() / 1000, seed;
    int ret;

    if (words < U_COUNT || user[U_MAGIC] != T1_ARMED)
        return 0;
    user[U_MAGIC] = 0;
    g_ran = 1;
    seed = user[U_SEED];
    probe_path(a, sizeof(a), 'a', seed);
    probe_path(b, sizeof(b), 'b', seed);

    /* ARK's mscache keeps one 4 KB window of msstor reads; any msstor call other than a read
       drops it (core/systemctrl/src/msstor_cache.c). A getstat on the raw device is harmless. */
    memset(&st, 0, sizeof(st));
    ret = sceIoGetstat("msstor0p1:/", &st);
    pw_log("cardtest: msstor getstat (drops ARK's mscache window) 0x%08X", ret);

    /* The Memory Stick may still be coming back from the suspend: retry while it errors. */
    for (;;) {
        memset(&st, 0, sizeof(st));
        ret = sceIoGetstat(a, &st);
        if (ret >= 0 || (unsigned int)ret == NOT_FOUND ||
            sceKernelGetSystemTimeLow() / 1000 - t0 > READY_WAIT_MS)
            break;
        sceKernelDelayThread(50 * 1000);
    }
    g_stat_ret = ret;
    g_wait_ms = (int)(sceKernelGetSystemTimeLow() / 1000 - t0);
    g_found = ret >= 0;
    g_size_ok = g_found && st.st_size == PROBE_BYTES;
    pw_log("cardtest: after the rollback: probe A %s (getstat 0x%08X after %d ms, size %u)",
           g_found ? "found" : "NOT FOUND", ret, g_wait_ms, (unsigned int)st.st_size);
    if (!g_size_ok)
        return -1;

    if (pw_mem_alloc(&mem, "PwCardTest", 2 * CHUNK) < 0)
        return -1;
    g_data_ok = verify_probe(a, seed, mem.ptr) == 0;
    pw_log("cardtest: probe A contents %s", g_data_ok ? "match" : "DIFFER");
    if (g_data_ok) {
        /* The real question: does a new allocation in the restored world collide with A? */
        g_b_written = write_probe(b, seed ^ 0xFFFFFFFFu, mem.ptr) == 0;
        g_a_still_ok = verify_probe(a, seed, mem.ptr) == 0;
        g_b_ok = g_b_written && verify_probe(b, seed ^ 0xFFFFFFFFu, mem.ptr) == 0;
        pw_log("cardtest: probe B written %d, read back %s; probe A afterwards %s", g_b_written,
               g_b_ok ? "ok" : "BAD", g_a_still_ok ? "ok" : "DAMAGED");
    }
    pw_mem_free(&mem);
    return g_data_ok && g_b_ok && g_a_still_ok ? 0 : -1;
}

int pw_cardtest_prepare(void)
{
    g_ran = g_found = g_size_ok = g_data_ok = g_b_written = g_a_still_ok = g_b_ok = 0;
    pw_engine_set_load_hook(check);
    pw_log("cardtest: T1 armed (the hook is part of the next snapshot)");
    return 0;
}

int pw_cardtest_write_probe(void)
{
    unsigned int *user = pw_engine_user_area(), seed = sceKernelGetSystemTimeLow() | 1u;
    char a[256];
    pw_mem_block mem;
    int ok;

    if (user == 0 || pw_mem_alloc(&mem, "PwCardTest", 2 * CHUNK) < 0)
        return -1;
    probe_path(a, sizeof(a), 'a', seed);
    /* Deliberately after the save: the saved kernel never saw this file or its clusters. */
    ok = write_probe(a, seed, mem.ptr) == 0 && verify_probe(a, seed, mem.ptr) == 0;
    pw_mem_free(&mem);
    if (!ok)
        return -1;
    user[U_SEED] = seed;
    user[U_BYTES] = PROBE_BYTES;
    user[U_MAGIC] = T1_ARMED;
    sceKernelDcacheWritebackAll();
    return 0;
}

void pw_cardtest_cancel(void)
{
    pw_engine_set_load_hook(0);
}

void pw_cardtest_result(char *line1, char *line2, unsigned int cap)
{
    pw_engine_set_load_hook(0);
    if (!g_ran) {
        stbsp_snprintf(line1, (int)cap, "T1: the check did not run.");
        stbsp_snprintf(line2, (int)cap, "Details are in pocketwatch.log.");
    } else if (g_data_ok && g_b_ok && g_a_still_ok) {
        stbsp_snprintf(line1, (int)cap, "T1 passed: the card was seen as it is now.");
        stbsp_snprintf(line2, (int)cap, "Connect USB and send the log.");
    } else {
        stbsp_snprintf(line1, (int)cap, "T1 FAILED: %s.",
                       !g_found     ? "the new file is invisible"
                       : !g_size_ok ? "the new file has the wrong size"
                       : !g_data_ok ? "the new file reads wrong"
                                    : "a new file damaged another");
        stbsp_snprintf(line2, (int)cap, "Do not save. Quit and restart the PSP.");
    }
}
