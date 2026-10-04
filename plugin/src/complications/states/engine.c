// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspge.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include <pspsysevent.h>
#include <pspsysmem_kernel.h>
#include <psputilsforkernel.h>
#include <string.h>

#include "complications/states/engine.h"
#include "complications/states/stream.h"
#include "core/crc32.h"
#include "core/flash.h"
#include "core/freeze.h"
#include "core/log.h"
#include "core/mem.h"
#include "stb_sprintf.h"

/* Memory map of a retail game on the PSP-3000 (docs/research/m1-hardware.md). */
#define KERNEL_BASE  0x88000000u
#define KERNEL_SIZE  0x00400000u /* P1 + P4 */
#define USER_BASE    0x88800000u /* P2 through the cached kernel segment */
#define USER_SIZE    0x01800000u /* 24 MB; anything else means highmem */
#define VOLATILE_PID 5
#define VOLATILE_KB  4096
#define VRAM_BASE    0x84000000u
#define SCRATCH_BASE 0x00010000u
#define SCRATCH_SIZE 0x00004000u
#define KSEG1        0x20000000u /* kseg0 address + KSEG1 = uncached alias */

#define EV_POWER_LOCK_PHASE1 0x00000401
#define EV_SUSPEND_PHASE0_0  0x00004000
#define EV_RESUME_COMPLETED  0x00400000

/* Layout of the P11 block: header, FastLZ workspace, arena..., apply blob. */
#define HEADER_SPACE    0x10000u
#define WORKSPACE_START 0x08000u
#define BLOB_SPACE      0x01000u
#define STATE_MAGIC     0x31535750u /* "PWS1" */

#define FREEZE_TIMEOUT_MS 500
#define GE_TIMEOUT_MS     100
#define RESUME_POLLS      2000 /* x 5 ms of active time; the sleep itself does not count */
#define WAKE_DELAY_US     100000ull
#define TRACE_MAX         128

#ifdef PW_DEBUG
#define VERIFY 1 /* checksum game RAM on save and after load */
#else
#define VERIFY 0
#endif

enum { MODE_IDLE, MODE_SAVE, MODE_LOAD };
enum { TRACE_SAVE, TRACE_LOAD, TRACE_AFTER_ROLLBACK, TRACE_KINDS };

/* Lives in P11, outside the kernel range: survives the rollback, readable by both sides. */
typedef struct {
    unsigned int magic;
    unsigned int valid;
    unsigned int load_applying; /* set by LOAD right before the rollback */
    pw_stream user, vram, scratch;
    unsigned int vram_size;
    unsigned int save_threads, save_forced, save_freeze_ms, save_pack_ms, save_suspend_ms;
    int save_ge;
    unsigned int save_dmac_left, save_capture_cycles;
    unsigned int load_threads, load_forced, load_freeze_ms;
    int load_ge;
    unsigned long long load_start_tick;
    unsigned int loads;
    unsigned int trace_count[TRACE_KINDS];
    unsigned int trace_ev[TRACE_KINDS][TRACE_MAX];
    unsigned int trace_cyc[TRACE_KINDS][TRACE_MAX];
} StateHeader;

_Static_assert(sizeof(StateHeader) <= WORKSPACE_START, "state header overlaps the workspace");
_Static_assert(WORKSPACE_START + PW_STREAM_WORKSPACE <= HEADER_SPACE, "workspace too big");

/* Everything below is in kernel RAM, i.e. part of the snapshot. */
volatile unsigned int pw_handoff_ctx[16];       /* written by pw_sysevent_shim (handoff.S) */
static volatile unsigned int g_restore_pending; /* LOAD sets it to 1 inside the staged image */
static volatile int g_mode, g_alarm_set, g_resumed, g_captured, g_applying;
static StateHeader *volatile g_hdr;
static unsigned char *volatile g_kernel_image;
static void *volatile g_blob;
static pw_mem_block g_stage_a, g_stage_b;
static pw_arena g_arena;
static pw_freeze_set g_save_freeze, g_load_freeze;
static unsigned int g_cycles_per_ms;
static pw_flash_report g_flash_report; /* filled at RESUME_COMPLETED after a rollback */
static volatile int g_flash_result;
static int g_registered;

extern int pw_sysevent_shim(int ev_id, char *ev_name, void *param, int *result);
extern char pw_apply_handoff[], pw_apply_handoff_end[];
int sceRtcSetAlarmTick(u64 *tick); /* sceRtc_driver, not declared by PSPSDK */
int pw_engine_sysevent(int ev_id, char *ev_name, void *param, int *result);

static PspSysEventHandler g_handler = {
    sizeof(PspSysEventHandler), "PocketwatchEngine", 0x00FFFF00, pw_sysevent_shim, 0, 0, NULL, {0},
};

static unsigned int cycles(void)
{
    unsigned int c;

    __asm__ volatile("mfc0 %0, $9" : "=r"(c)); /* CP0 Count */
    return c;
}

static unsigned int now_ms(void)
{
    return sceKernelGetSystemTimeLow() / 1000;
}

/* Wait until both DMA controllers are idle so memory is settled before copying it. */
static unsigned int dmac_drain(void)
{
    volatile unsigned int *en0 = (volatile unsigned int *)(0xBC900000 + 0x1C);
    volatile unsigned int *en1 = (volatile unsigned int *)(0xBCA00000 + 0x1C);
    unsigned int spin = 0;

    while ((*en0 | *en1) && spin < 200000)
        spin++;
    return *en0 | *en1;
}

static void trace(StateHeader *h, int kind, unsigned int ev)
{
    unsigned int n = h->trace_count[kind];

    if (n < TRACE_MAX) {
        h->trace_ev[kind][n] = ev;
        h->trace_cyc[kind][n] = cycles();
        h->trace_count[kind] = n + 1;
    }
}

/*
 * Runs in the power manager's thread for every system event (entered through pw_sysevent_shim).
 * Memory operations only: no I/O, no logging.
 */
int pw_engine_sysevent(int ev_id, char *ev_name, void *param, int *result)
{
    StateHeader *h = g_hdr;

    (void)ev_name;
    (void)param;
    (void)result;
    if (g_mode == MODE_IDLE || h == NULL)
        return 0;

    trace(h,
          g_mode == MODE_LOAD ? TRACE_LOAD
          : h->load_applying  ? TRACE_AFTER_ROLLBACK
                              : TRACE_SAVE,
          (unsigned int)ev_id);

    if (ev_id == EV_POWER_LOCK_PHASE1 && !g_alarm_set) {
        u64 tick;

        sceRtcGetCurrentTick(&tick);
        tick += WAKE_DELAY_US;
        sceRtcSetAlarmTick(&tick);
        g_alarm_set = 1;
    } else if (ev_id == EV_SUSPEND_PHASE0_0) {
        if (g_mode == MODE_SAVE && !g_captured) {
            unsigned int c0 = cycles();

            h->save_dmac_left = dmac_drain();
            memcpy(g_kernel_image, (const void *)KERNEL_BASE, KERNEL_SIZE);
            sceKernelDcacheWritebackAll(); /* the image must be in DRAM before power-down */
            h->save_capture_cycles = cycles() - c0;
            g_captured = 1;
        } else if (g_mode == MODE_LOAD && !g_applying) {
            void (*apply)(const void *, void *, unsigned int, const volatile void *) =
                (void *)((unsigned int)g_blob | KSEG1);

            g_applying = 1;
            dmac_drain();
            sceKernelCpuSuspendIntr();
            apply(g_kernel_image, (void *)KERNEL_BASE, KERNEL_SIZE, pw_handoff_ctx);
            /* not reached: execution continues in the SAVE's suspend */
        }
    } else if (ev_id == EV_RESUME_COMPLETED) {
        sceRtcSetAlarmTick(NULL); /* never wake the next natural sleep early */
        if (g_restore_pending)    /* back from a rollback: fix flash before anyone else uses it */
            g_flash_result = pw_flash_reinit(&g_flash_report);
        g_resumed = 1;
    }
    return 0;
}

static unsigned int calibrate(void)
{
    if (g_cycles_per_ms == 0) {
        unsigned int c0 = cycles();

        sceKernelDelayThread(50 * 1000);
        g_cycles_per_ms = (cycles() - c0) / 50;
    }
    return g_cycles_per_ms ? g_cycles_per_ms : 1;
}

/* The game must fit the memory map this engine saves. */
static int check_environment(void)
{
    PspSysmemPartitionInfo p2;

    memset(&p2, 0, sizeof(p2));
    p2.size = sizeof(p2);
    if (sceKernelQueryMemoryPartitionInfo(2, &p2) < 0 || p2.memsize != USER_SIZE) {
        pw_log("engine: game RAM is %u KB, expected %u KB (highmem?): not supported",
               p2.memsize / 1024, USER_SIZE / 1024);
        return -1;
    }
    if (sceKernelPartitionTotalFreeMemSize(VOLATILE_PID) / 1024 < VOLATILE_KB) {
        pw_log("engine: the game uses volatile memory (P5): not supported yet");
        return -1;
    }
    return 0;
}

static void release_staging(void)
{
    g_hdr = NULL;
    g_kernel_image = NULL;
    g_blob = NULL;
    pw_stream_set_workspace(NULL);
    pw_mem_free(&g_stage_a);
    pw_mem_free(&g_stage_b);
}

/* P9: kernel image + arena region 0. P11: header + workspace, arena region 1, apply blob. */
static int ensure_staging(void)
{
    unsigned int size_a, size_b, blob_len = (unsigned int)(pw_apply_handoff_end - pw_apply_handoff);
    int ret;

    if (g_hdr != NULL)
        return 0;
    if (blob_len > BLOB_SPACE)
        return -1;
    size_a = pw_mem_max_free(PW_MEM_PARTITION_EXTRA) & ~0xFFFFu;
    size_b = pw_mem_max_free(PW_MEM_PARTITION_SPARE) & ~0xFFFFu;
    if (size_a < KERNEL_SIZE + 0x100000 || size_b < HEADER_SPACE + BLOB_SPACE + 0x10000) {
        pw_log("engine: not enough spare RAM (P9 %u KB, P11 %u KB)", size_a / 1024, size_b / 1024);
        return -1;
    }
    if ((ret = pw_mem_alloc_in(&g_stage_a, PW_MEM_PARTITION_EXTRA, "PwStateA", size_a)) < 0 ||
        (ret = pw_mem_alloc_in(&g_stage_b, PW_MEM_PARTITION_SPARE, "PwStateB", size_b)) < 0) {
        pw_log("engine: staging alloc failed 0x%08X", ret);
        release_staging();
        return -1;
    }

    g_kernel_image = g_stage_a.ptr;
    g_hdr = g_stage_b.ptr;
    g_blob = (unsigned char *)g_stage_b.ptr + size_b - BLOB_SPACE;
    pw_stream_set_workspace((unsigned char *)g_stage_b.ptr + WORKSPACE_START);
    g_arena.base[0] = (unsigned char *)g_stage_a.ptr + KERNEL_SIZE;
    g_arena.size[0] = size_a - KERNEL_SIZE;
    g_arena.base[1] = (unsigned char *)g_stage_b.ptr + HEADER_SPACE;
    g_arena.size[1] = size_b - HEADER_SPACE - BLOB_SPACE;
    return 0;
}

/* With the game frozen, let the graphics engine finish (or stall on) its current list. */
static int ge_wait_idle(void)
{
    unsigned int t0 = now_ms();
    int st;

    for (;;) {
        st = sceGeDrawSync(1);
        if (st < 0 || st == PSP_GE_LIST_DONE || st == PSP_GE_LIST_STALL_REACHED)
            return st;
        if (now_ms() - t0 >= GE_TIMEOUT_MS)
            return st;
        sceKernelDelayThread(1000);
    }
}

static void log_stream(const char *name, const pw_stream *s)
{
    pw_log("engine:   %-7s %6u KB -> %6u KB (%u chunks, %u raw) crc %08X", name, s->raw_len / 1024,
           s->packed_len / 1024, s->chunks, s->raw_chunks, s->crc32);
}

/* Event trace; runs of a repeated 1- or 2-event pattern are folded into "A B xN". */
static void log_trace(const StateHeader *h, int kind, const char *label)
{
    static const char digits[] = "0123456789ABCDEF";
    unsigned int count = h->trace_count[kind], i = 0;
    char line[200];
    int len = 0;

    pw_log("engine: %s trace, %u event(s):", label, count);
    while (i < count) {
        unsigned int width = 1, repeats = 1;

        for (unsigned int w = 2; w >= 1; w--) {
            unsigned int r = 1;

            while (i + (r + 1) * w <= count &&
                   h->trace_ev[kind][i + r * w] == h->trace_ev[kind][i] &&
                   (w == 1 || h->trace_ev[kind][i + r * w + 1] == h->trace_ev[kind][i + 1]))
                r++;
            if (r >= 3) {
                width = w;
                repeats = r;
                break;
            }
        }
        for (unsigned int k = 0; k < width; k++) {
            unsigned int v = h->trace_ev[kind][i + k];

            line[len++] = ' ';
            for (int sft = 20; sft >= 0; sft -= 4)
                line[len++] = digits[(v >> sft) & 15];
        }
        if (repeats > 1)
            len += stbsp_snprintf(line + len, sizeof(line) - (unsigned int)len, " x%u", repeats);
        i += width * repeats;
        if (len > 150 || i >= count) {
            line[len] = '\0';
            pw_log("engine: %s", line);
            len = 0;
        }
    }
    if (count >= 2)
        pw_log("engine: %s trace spans %u ms (CPU time, the sleep itself excluded)", label,
               (h->trace_cyc[kind][count - 1] - h->trace_cyc[kind][0]) / calibrate());
}

/* Back in the SAVE-time thread after a LOAD rolled the kernel back. */
static int finish_load(void)
{
    StateHeader *h = g_hdr;
    unsigned int t0 = now_ms(), unpack_ms, verify_crc = 0;
    const pw_flash_report *f = &g_flash_report;
    int ok;
    u64 now;

    ok = pw_stream_read(&g_arena, &h->user, (void *)USER_BASE, USER_SIZE) >= 0;
    ok &= pw_stream_read(&g_arena, &h->vram, (void *)VRAM_BASE, h->vram_size) >= 0;
    ok &= pw_stream_read(&g_arena, &h->scratch, (void *)SCRATCH_BASE, SCRATCH_SIZE) >= 0;
    sceKernelDcacheWritebackInvalidateAll();
    sceKernelIcacheInvalidateAll(); /* the game's code in RAM was replaced too */
    unpack_ms = now_ms() - t0;
    if (VERIFY)
        verify_crc = pw_crc32(0, (const void *)USER_BASE, USER_SIZE);

    g_restore_pending = 0;
    h->load_applying = 0;
    h->loads++;
    pw_thaw_game(&g_save_freeze);

    sceRtcGetCurrentTick(&now);
    pw_log("engine: ---- LOAD #%u complete: %s ----", h->loads, ok ? "OK" : "DECOMPRESSION FAILED");
    pw_log("engine: load: froze %u thread(s) in %u ms (%u forced), GE %d", h->load_threads,
           h->load_freeze_ms, h->load_forced, h->load_ge);
    pw_log("engine: load: request -> game running %u ms (wall clock), unpack %u ms",
           (unsigned int)((now - h->load_start_tick) / 1000), unpack_ms);
    pw_log("engine: load: flash %d: drivers %d/%d, unassign %X/%X, exit %X/%X, init %X/%X, "
           "assign %X/%X",
           g_flash_result, f->found_lflash, f->found_flashfat, f->unassign0, f->unassign1,
           f->exit_lflash, f->exit_flashfat, f->init_lflash, f->init_flashfat, f->assign0,
           f->assign1);
    if (VERIFY)
        pw_log("engine: load: game RAM crc %08X, saved %08X: %s", verify_crc, h->user.crc32,
               verify_crc == h->user.crc32 ? "IDENTICAL" : "DIFFERENT");
    log_trace(h, TRACE_LOAD, "load (before rollback)");
    log_trace(h, TRACE_AFTER_ROLLBACK, "load (after rollback)");
    return PW_ENGINE_LOADED;
}

int pw_engine_save(void)
{
    StateHeader *h;
    pw_arena_pos cursor = {0, 0};
    unsigned int t0;
    u64 rt0 = 0, rt1 = 0;
    int ret, polls = 0;

    if (!g_registered)
        return -1;
    if (pw_engine_has_state())
        pw_engine_discard(); /* a single slot for now */
    pw_log("engine: ---- SAVE ----");
    if (check_environment() < 0 || ensure_staging() < 0)
        return -1;

    h = g_hdr;
    memset(h, 0, sizeof(*h));
    h->magic = STATE_MAGIC;
    calibrate();

    pw_freeze_game(&g_save_freeze, FREEZE_TIMEOUT_MS);
    h->save_threads = (unsigned int)g_save_freeze.count;
    h->save_forced = (unsigned int)g_save_freeze.forced;
    h->save_freeze_ms = g_save_freeze.ms;
    if (g_save_freeze.count == 0) {
        pw_log("engine: no game threads to freeze");
        release_staging();
        return -1;
    }
    h->save_ge = ge_wait_idle();
    h->vram_size = sceGeEdramGetSize();

    t0 = now_ms();
    if (pw_stream_write(&g_arena, &cursor, (const void *)USER_BASE, USER_SIZE, VERIFY, &h->user) <
            0 ||
        pw_stream_write(&g_arena, &cursor, (const void *)VRAM_BASE, h->vram_size, VERIFY,
                        &h->vram) < 0 ||
        pw_stream_write(&g_arena, &cursor, (const void *)SCRATCH_BASE, SCRATCH_SIZE, VERIFY,
                        &h->scratch) < 0) {
        pw_thaw_game(&g_save_freeze);
        pw_log("engine: the state does not fit in spare RAM");
        release_staging();
        return -1;
    }
    h->save_pack_ms = now_ms() - t0;
    pw_freeze_log(&g_save_freeze);
    pw_log("engine: save: GE %d, packed in %u ms:", h->save_ge, h->save_pack_ms);
    log_stream("game", &h->user);
    log_stream("eDRAM", &h->vram);
    log_stream("scratch", &h->scratch);

    /* From the kernel snapshot on, the Memory Stick must not change until the state is dropped. */
    pw_log_flush();
    pw_log_set_deferred(1);

    g_restore_pending = 0;
    g_alarm_set = g_resumed = g_captured = g_applying = 0;
    g_mode = MODE_SAVE;
    sceKernelDcacheWritebackAll();
    sceRtcGetCurrentTick(&rt0);
    ret = scePowerRequestSuspend();
    while (!g_resumed && polls++ < RESUME_POLLS)
        sceKernelDelayThread(5000);
    sceRtcGetCurrentTick(&rt1);
    g_mode = MODE_IDLE;

    if (g_restore_pending)
        return finish_load();

    pw_thaw_game(&g_save_freeze);
    h->save_suspend_ms = (unsigned int)((rt1 - rt0) / 1000);
    if (ret < 0 || !g_resumed || !g_captured) {
        if (!g_resumed)
            sceRtcSetAlarmTick(NULL);
        pw_log_set_deferred(0);
        pw_log("engine: save FAILED: suspend=0x%08X resumed=%d captured=%d", ret, g_resumed,
               g_captured);
        log_trace(h, TRACE_SAVE, "save");
        release_staging();
        return -1;
    }

    h->valid = 1;
    pw_log("engine: save OK: suspend %u ms (wall clock), kernel copy %u ms, dmac_left=0x%X",
           h->save_suspend_ms, h->save_capture_cycles / calibrate(), h->save_dmac_left);
    log_trace(h, TRACE_SAVE, "save");
    pw_log("engine: state held in RAM; log lines are kept in RAM until it is discarded");
    return PW_ENGINE_SAVED;
}

int pw_engine_load(void)
{
    StateHeader *h = g_hdr;
    volatile unsigned int *pending_in_image;
    int ret, polls = 0;

    if (!pw_engine_has_state()) {
        pw_log("engine: LOAD: no state");
        return -1;
    }
    pw_log("engine: ---- LOAD ----");
    pending_in_image = (volatile unsigned int *)(g_kernel_image +
                                                 ((unsigned int)&g_restore_pending - KERNEL_BASE));

    pw_freeze_game(&g_load_freeze, FREEZE_TIMEOUT_MS);
    h->load_threads = (unsigned int)g_load_freeze.count;
    h->load_forced = (unsigned int)g_load_freeze.forced;
    h->load_freeze_ms = g_load_freeze.ms;
    h->load_ge = ge_wait_idle();

    /* The SAVE-time thread must learn it was brought back by a LOAD: flag it inside the image. */
    *pending_in_image = 1;
    memcpy(g_blob, pw_apply_handoff, (unsigned int)(pw_apply_handoff_end - pw_apply_handoff));
    h->trace_count[TRACE_LOAD] = 0;
    h->trace_count[TRACE_AFTER_ROLLBACK] = 0;
    h->load_applying = 1;
    sceRtcGetCurrentTick(&h->load_start_tick);

    g_alarm_set = g_resumed = g_captured = g_applying = 0;
    g_mode = MODE_LOAD;
    sceKernelDcacheWritebackAll(); /* the blob and the flag must be in DRAM */
    ret = scePowerRequestSuspend();
    while (!g_resumed && polls++ < RESUME_POLLS)
        sceKernelDelayThread(5000);

    /* Still here: PHASE0_0 never came (suspend refused or timed out). Undo and report. */
    g_mode = MODE_IDLE;
    h->load_applying = 0;
    *pending_in_image = 0;
    if (!g_resumed)
        sceRtcSetAlarmTick(NULL);
    pw_thaw_game(&g_load_freeze);
    pw_log("engine: LOAD FAILED: suspend=0x%08X resumed=%d applying=%d", ret, g_resumed,
           g_applying);
    log_trace(h, TRACE_LOAD, "load");
    return -1;
}

int pw_engine_has_state(void)
{
    return g_hdr != NULL && g_hdr->valid;
}

void pw_engine_discard(void)
{
    unsigned int loads = g_hdr ? g_hdr->loads : 0;

    release_staging();
    pw_log_set_deferred(0);
    pw_log("engine: state discarded after %u load(s)", loads);
    pw_log_flush();
}

int pw_engine_init(void)
{
    int ret = sceKernelRegisterSysEventHandler(&g_handler);

    g_registered = ret >= 0;
    return ret;
}

void pw_engine_shutdown(void)
{
    if (g_registered)
        sceKernelUnregisterSysEventHandler(&g_handler);
    g_registered = 0;
    release_staging();
}
