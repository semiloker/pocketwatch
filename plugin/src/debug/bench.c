// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspkernel.h>
#include <psppower.h>
#include <string.h>

#include "complications/states/engine.h"
#include "complications/states/stream.h"
#include "core/crc32.h"
#include "core/freeze.h"
#include "core/log.h"
#include "core/mem.h"
#include "debug/bench.h"

#define USER_BASE   0x88800000u /* game RAM (P2) through the cached kernel segment */
#define USER_SIZE   0x01800000u
#define UNCACHED(a) ((a) | 0x20000000u)
#define SLICE       0x00400000u /* work in 4 MB slices: P9 cannot hold 24 MB twice */
#define SLICES      (USER_SIZE / SLICE)
#define ARENA_SIZE  (SLICE + SLICE / 8)
#define PAGE        4096u
#define WS_SIZE     PW_STREAM_WORKSPACE

static unsigned int now_us(void)
{
    return sceKernelGetSystemTimeLow();
}

/* "name  24576 KB in 123 ms = 199.8 MB/s" (MB = 10^6 bytes, as bytes per microsecond). */
static void report(const char *what, unsigned int bytes, unsigned int us)
{
    unsigned int x10 = us ? (unsigned int)((unsigned long long)bytes * 10 / us) : 0;

    pw_log("bench: %-26s %6u KB in %5u ms = %4u.%u MB/s", what, bytes / 1024, us / 1000, x10 / 10,
           x10 % 10);
}

static void copy_words(unsigned int *d, const unsigned int *s, unsigned int bytes)
{
    for (unsigned int n = bytes / 32; n; n--, d += 8, s += 8) {
        unsigned int a = s[0], b = s[1], c = s[2], e = s[3], f = s[4], g = s[5], h = s[6], i = s[7];

        d[0] = a;
        d[1] = b;
        d[2] = c;
        d[3] = e;
        d[4] = f;
        d[5] = g;
        d[6] = h;
        d[7] = i;
    }
}

/* Destination lines are allocated with "create dirty exclusive" instead of being read first. */
static void copy_cde(unsigned int *d, const unsigned int *s, unsigned int bytes)
{
    for (unsigned int n = bytes / 64; n; n--, d += 16, s += 16) {
        __asm__ volatile("cache 0x18, 0(%0)" : : "r"(d) : "memory");
        for (int i = 0; i < 16; i++)
            d[i] = s[i];
    }
}

/* 1 when every word of the page equals the first one (a FILL record would cover it). */
static int page_is_uniform(const unsigned int *w)
{
    unsigned int v = w[0];

    for (unsigned int i = 1; i < PAGE / 4; i++)
        if (w[i] != v)
            return 0;
    return 1;
}

static int page_is_zero(const unsigned int *w)
{
    unsigned int acc = 0;

    for (unsigned int i = 0; i < PAGE / 4; i += 4)
        acc |= w[i] | w[i + 1] | w[i + 2] | w[i + 3];
    return acc == 0;
}

static void bench_copies(unsigned char *buf)
{
    unsigned int t0, sink;

    t0 = now_us();
    for (unsigned int k = 0; k < SLICES; k++)
        memcpy(buf, (const void *)(USER_BASE + k * SLICE), SLICE);
    report("memcpy (sysclib) cached", USER_SIZE, now_us() - t0);

    t0 = now_us();
    for (unsigned int k = 0; k < SLICES; k++)
        copy_words((unsigned int *)buf, (const unsigned int *)(USER_BASE + k * SLICE), SLICE);
    report("word copy x8 cached", USER_SIZE, now_us() - t0);

    t0 = now_us();
    for (unsigned int k = 0; k < SLICES; k++)
        copy_cde((unsigned int *)buf, (const unsigned int *)(USER_BASE + k * SLICE), SLICE);
    report("copy + cache 0x18 dst", USER_SIZE, now_us() - t0);

    t0 = now_us();
    memset(buf, 0, SLICE);
    report("memset", SLICE, now_us() - t0);

    t0 = now_us();
    sink = pw_sum32((const void *)UNCACHED(USER_BASE), SLICE);
    report("sum32 uncached read", SLICE, now_us() - t0);

    t0 = now_us();
    sink += pw_sum32((const void *)USER_BASE, USER_SIZE);
    report("sum32 cached", USER_SIZE, now_us() - t0);

    t0 = now_us();
    sink += pw_crc32(0, (const void *)USER_BASE, USER_SIZE);
    report("crc32 bytewise", USER_SIZE, now_us() - t0);
    pw_log("bench: (checksum sink %08X)", sink);
}

static void bench_zero_pages(void)
{
    unsigned int t0 = now_us(), zero = 0, zero_chunks = 0;

    for (unsigned int c = 0; c < USER_SIZE / PW_STREAM_CHUNK; c++) {
        unsigned int z = 0;

        for (unsigned int p = 0; p < PW_STREAM_CHUNK / PAGE; p++)
            z += page_is_zero((const unsigned int *)(USER_BASE + c * PW_STREAM_CHUNK + p * PAGE));
        zero += z;
        zero_chunks += z == PW_STREAM_CHUNK / PAGE;
    }
    report("zero-page scan", USER_SIZE, now_us() - t0);
    pw_log("bench: zero pages %u of %u (%u%%), all-zero 64 KB chunks %u of %u", zero,
           USER_SIZE / PAGE, zero * 100 / (USER_SIZE / PAGE), zero_chunks,
           USER_SIZE / PW_STREAM_CHUNK);

    unsigned int uniform = 0;

    t0 = now_us();
    for (unsigned int p = 0; p < USER_SIZE / PAGE; p++)
        uniform += page_is_uniform((const unsigned int *)(USER_BASE + p * PAGE));
    report("uniform-page scan", USER_SIZE, now_us() - t0);
    pw_log("bench: uniform pages (FILL candidates) %u of %u (%u%%)", uniform, USER_SIZE / PAGE,
           uniform * 100 / (USER_SIZE / PAGE));
}

/* The engine's stream code (LZ4 + FILL), slice by slice; each slice is unpacked and compared. */
static void bench_stream(unsigned char *buf, unsigned char *arena_mem, int with_crc)
{
    unsigned int t_pack = 0, t_unpack = 0, packed = 0, bad = 0, fills = 0, raws = 0;
    pw_arena arena = {{arena_mem, NULL}, {ARENA_SIZE, 0}};

    for (unsigned int k = 0; k < SLICES; k++) {
        const void *src = (const void *)(USER_BASE + k * SLICE);
        pw_arena_pos cursor = {0, 0};
        pw_stream s;
        unsigned int t0 = now_us();

        if (pw_stream_write(&arena, &cursor, src, SLICE, with_crc, &s) < 0) {
            pw_log("bench: slice %u does not fit the arena", k);
            return;
        }
        t_pack += now_us() - t0;
        packed += s.packed_len;
        fills += s.fill_chunks;
        raws += s.raw_chunks;
        t0 = now_us();
        if (pw_stream_read(&arena, &s, buf, SLICE) < 0)
            bad++;
        t_unpack += now_us() - t0;
        bad += memcmp(buf, src, SLICE) != 0;
    }
    report(with_crc ? "stream pack + crc32" : "stream pack (lz4+fill)", USER_SIZE, t_pack);
    report("stream unpack", USER_SIZE, t_unpack);
    pw_log("bench: stream %u KB -> %u KB (%u%%), %u fill / %u raw chunk(s), %u slice(s) not "
           "identical after unpack",
           USER_SIZE / 1024, packed / 1024, packed * 100 / USER_SIZE, fills, raws, bad);
}

int pw_bench_run(void)
{
    pw_mem_block buf, arena, ws;
    pw_freeze_set frozen;
    int ret;

    if (pw_engine_has_state()) {
        pw_log("bench: a state is held; drop it first (L+R+SELECT+UP)");
        return -1;
    }
    if ((ret = pw_mem_alloc_in(&buf, PW_MEM_PARTITION_EXTRA, "PwBenchBuf", SLICE)) < 0)
        return ret;
    if ((ret = pw_mem_alloc_in(&arena, PW_MEM_PARTITION_EXTRA, "PwBenchArena", ARENA_SIZE)) < 0 ||
        (ret = pw_mem_alloc_in(&ws, PW_MEM_PARTITION_EXTRA, "PwBenchWs", WS_SIZE)) < 0) {
        pw_mem_free(&arena);
        pw_mem_free(&buf);
        return ret;
    }

    pw_freeze_game(&frozen, 500);
    pw_log("bench: ---- start: CPU %d MHz, bus %d MHz, %d game thread(s) frozen ----",
           scePowerGetCpuClockFrequency(), scePowerGetBusClockFrequency(), frozen.count);
    bench_copies(buf.ptr);
    bench_zero_pages();
    pw_stream_set_workspace(ws.ptr);
    bench_stream(buf.ptr, arena.ptr, 0);
    bench_stream(buf.ptr, arena.ptr, 1);
    pw_stream_set_workspace(NULL);
    pw_thaw_game(&frozen);
    pw_log("bench: ---- end ----");

    pw_mem_free(&ws);
    pw_mem_free(&arena);
    pw_mem_free(&buf);
    return 0;
}
