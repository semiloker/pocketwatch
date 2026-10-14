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
#define P8_PID       8
#define P8_SPACE     0x00100000u /* P8 (1 MB on the PSP-3000) is outside the kernel range */

#define EV_POWER_LOCK_PHASE1 0x00000401
#define EV_SUSPEND_PHASE0_0  0x00004000
#define EV_RESUME_COMPLETED  0x00400000

/* Layout of the P11 block: header, FastLZ workspace, arena..., apply blob. */
#define HEADER_SPACE    0x10000u
#define WORKSPACE_START 0x08000u
#define BLOB_SPACE      0x01000u
#define STATE_MAGIC     0x32535750u /* "PWS2": layout with breadcrumbs */

#define FREEZE_TIMEOUT_MS 500
#define GE_TIMEOUT_MS     100
#define RESUME_POLLS      2000 /* x 5 ms of active time; the sleep itself does not count */
#define WAKE_DELAY_US     100000ull
/* LOAD and the self-test spend up to ~1 s copying the kernel (uncached code) after the alarm is
   armed; an alarm that expires before the CPU sleeps might never wake it. */
#define WAKE_DELAY_SLOW_US 1500000ull
/* Same-session loads do not need it (flash is unchanged); off while the load path is debugged. */
#define FLASH_REINIT_ON_LOAD 0
#define TRACE_MAX            128

#ifdef PW_DEBUG
#define VERIFY 1 /* checksum game RAM on save and after load */
#else
#define VERIFY 0
#endif

enum { MODE_IDLE, MODE_SAVE, MODE_LOAD, MODE_SELFTEST };
enum { TRACE_SAVE, TRACE_LOAD, TRACE_AFTER_ROLLBACK, TRACE_KINDS };

/*
 * Breadcrumbs, kept in the P11 header: they survive the rollback and, with luck, a crash back to
 * the XMB, so the next boot can report how far the previous save/load got (pw_engine_init).
 */
enum {
    S_SAVE_START = 10,
    S_SAVE_FROZEN,
    S_SAVE_PACKED,
    S_SAVE_SUSPEND,
    S_SAVE_CAPTURED, /* handler, PHASE0_0 */
    S_SAVE_RESUMED,  /* handler, RESUME_COMPLETED */
    S_SAVE_DONE,
    S_LOAD_START = 20,
    S_LOAD_FROZEN,
    S_LOAD_ARMED,
    S_LOAD_SUSPEND,
    S_LOAD_APPLY,    /* handler, PHASE0_0, jumping to the blob */
    S_LOAD_FAILED,   /* suspend never reached PHASE0_0 */
    S_RB_EVENT = 30, /* handler: first event after the rollback */
    S_RB_RESUME,     /* handler: RESUME_COMPLETED after the rollback */
    S_RB_FLASH,      /* handler: flash drivers re-initialised */
    S_RB_THREAD,     /* the save-time thread is running again */
    S_RB_RAM,        /* game RAM decompressed */
    S_RB_VRAM,       /* eDRAM decompressed */
    S_RB_CACHES,     /* caches written back / invalidated */
    S_RB_THAWED,     /* game threads resumed */
    S_LOAD_DONE,
    S_ST_START = 40, /* rollback self-test: re-apply the live kernel through the blob */
    S_ST_APPLY,
    S_ST_BACK, /* first event after the blob returned */
};

#define CRUMB_MAX 48

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
    int ge_selftest;
    unsigned int stage; /* last breadcrumb */
    unsigned int crumb_count;
    unsigned int crumb[CRUMB_MAX];
    unsigned int crumb_cyc[CRUMB_MAX];
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
static int g_keep_frozen;              /* debug: pw_engine_save leaves the game frozen */
static pw_flash_report g_flash_report; /* filled at RESUME_COMPLETED after a rollback */
static volatile int g_flash_result;
static int g_registered;
/* Partition 8 holds live data of something loaded at boot (343 KB used on the PSP-3000) but sits
   outside 0x88000000-0x88400000: it is captured and restored together with the kernel image. */
static unsigned int g_p8_base, g_p8_size;

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

/* Memory only: safe in the sysevent handler. */
static void crumb(StateHeader *h, unsigned int stage)
{
    unsigned int n = h->crumb_count;

    h->stage = stage;
    if (n < CRUMB_MAX) {
        h->crumb[n] = stage;
        h->crumb_cyc[n] = cycles();
        h->crumb_count = n + 1;
    }
}

static const char *stage_name(unsigned int s)
{
    switch (s) {
    case S_SAVE_START:
        return "save:start";
    case S_SAVE_FROZEN:
        return "save:frozen";
    case S_SAVE_PACKED:
        return "save:packed";
    case S_SAVE_SUSPEND:
        return "save:suspend-requested";
    case S_SAVE_CAPTURED:
        return "save:kernel-captured";
    case S_SAVE_RESUMED:
        return "save:resumed";
    case S_SAVE_DONE:
        return "save:done(state held)";
    case S_LOAD_START:
        return "load:start";
    case S_LOAD_FROZEN:
        return "load:frozen";
    case S_LOAD_ARMED:
        return "load:armed";
    case S_LOAD_SUSPEND:
        return "load:suspend-requested";
    case S_LOAD_APPLY:
        return "load:applying-kernel";
    case S_LOAD_FAILED:
        return "load:failed(no PHASE0_0)";
    case S_RB_EVENT:
        return "rollback:first-event";
    case S_RB_RESUME:
        return "rollback:resume-completed";
    case S_RB_FLASH:
        return "rollback:flash-reinit";
    case S_RB_THREAD:
        return "rollback:save-thread-running";
    case S_RB_RAM:
        return "rollback:game-ram-restored";
    case S_RB_VRAM:
        return "rollback:edram-restored";
    case S_RB_CACHES:
        return "rollback:caches-flushed";
    case S_RB_THAWED:
        return "rollback:game-thawed";
    case S_LOAD_DONE:
        return "load:done";
    case S_ST_START:
        return "selftest:start";
    case S_ST_APPLY:
        return "selftest:applying-live-kernel";
    case S_ST_BACK:
        return "selftest:back-in-dispatcher";
    default:
        return "?";
    }
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
    if (g_mode == MODE_SAVE && h->load_applying && h->stage < S_RB_EVENT)
        crumb(h, S_RB_EVENT); /* we are running inside the restored kernel */
    if (g_mode == MODE_SELFTEST && g_applying && h->stage == S_ST_APPLY)
        crumb(h, S_ST_BACK);

    if (ev_id == EV_POWER_LOCK_PHASE1 && !g_alarm_set) {
        u64 tick;

        sceRtcGetCurrentTick(&tick);
        tick += g_mode == MODE_SAVE ? WAKE_DELAY_US : WAKE_DELAY_SLOW_US;
        sceRtcSetAlarmTick(&tick);
        g_alarm_set = 1;
    } else if (ev_id == EV_SUSPEND_PHASE0_0) {
        if (g_mode == MODE_SAVE && !g_captured) {
            unsigned int c0 = cycles(), intr;

            h->save_dmac_left = dmac_drain();
            intr = sceKernelCpuSuspendIntr(); /* no interrupt may change the kernel mid-copy */
            memcpy(g_kernel_image, (const void *)KERNEL_BASE, KERNEL_SIZE);
            if (g_p8_size)
                memcpy(g_kernel_image + KERNEL_SIZE, (const void *)g_p8_base, g_p8_size);
            sceKernelCpuResumeIntr(intr);
            sceKernelDcacheWritebackAll(); /* the image must be in DRAM before power-down */
            h->save_capture_cycles = cycles() - c0;
            g_captured = 1;
            crumb(h, S_SAVE_CAPTURED);
        } else if (g_mode == MODE_LOAD && !g_applying) {
            void (*apply)(const void *, void *, unsigned int, const volatile void *) =
                (void *)((unsigned int)g_blob | KSEG1);

            g_applying = 1;
            crumb(h, S_LOAD_APPLY); /* written back by the blob's cache flush */
            if (g_p8_size)          /* nothing else runs at PHASE0_0: safe to put P8 back first */
                memcpy((void *)g_p8_base, g_kernel_image + KERNEL_SIZE, g_p8_size);
            dmac_drain();
            sceKernelCpuSuspendIntr();
            apply(g_kernel_image, (void *)KERNEL_BASE, KERNEL_SIZE, pw_handoff_ctx);
            /* not reached: execution continues in the SAVE's suspend */
        } else if (g_mode == MODE_SELFTEST && !g_applying) {
            void (*apply)(const void *, void *, unsigned int, const volatile void *) =
                (void *)((unsigned int)g_blob | KSEG1);

            g_applying = 1;
            crumb(h, S_ST_APPLY);
            dmac_drain();
            sceKernelCpuSuspendIntr();
            /* Snapshot the live kernel and put it straight back: the rollback minus the world
             * switch. */
            memcpy(g_kernel_image, (const void *)KERNEL_BASE, KERNEL_SIZE);
            apply(g_kernel_image, (void *)KERNEL_BASE, KERNEL_SIZE, pw_handoff_ctx);
            /* not reached: the blob returns into this dispatch */
        }
    } else if (ev_id == EV_RESUME_COMPLETED) {
        sceRtcSetAlarmTick(NULL); /* never wake the next natural sleep early */
        if (g_restore_pending) {  /* back from a rollback: fix flash before anyone else uses it */
            crumb(h, S_RB_RESUME);
            if (FLASH_REINIT_ON_LOAD) {
                g_flash_result = pw_flash_reinit(&g_flash_report);
                crumb(h, S_RB_FLASH);
            }
        } else if (g_mode == MODE_SAVE) {
            crumb(h, S_SAVE_RESUMED);
        }
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

    PspSysmemPartitionInfo p8;

    memset(&p8, 0, sizeof(p8));
    p8.size = sizeof(p8);
    if (sceKernelQueryMemoryPartitionInfo(P8_PID, &p8) >= 0 && p8.memsize <= P8_SPACE) {
        g_p8_base = p8.startaddr;
        g_p8_size = p8.memsize;
    } else {
        g_p8_base = 0;
        g_p8_size = 0;
    }
    return 0;
}

static void release_staging(void)
{
    if (g_hdr != NULL)
        g_hdr->magic = 0; /* no post-mortem for a state we dropped on purpose */
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
    if (size_a < KERNEL_SIZE + P8_SPACE + 0x100000 ||
        size_b < HEADER_SPACE + BLOB_SPACE + 0x10000) {
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
    g_arena.base[0] = (unsigned char *)g_stage_a.ptr + KERNEL_SIZE + P8_SPACE;
    g_arena.size[0] = size_a - KERNEL_SIZE - P8_SPACE;
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
static void log_trace_raw(const unsigned int *ev, const unsigned int *cyc, unsigned int count,
                          const char *label)
{
    static const char digits[] = "0123456789ABCDEF";
    unsigned int i = 0;
    char line[200];
    int len = 0;

    if (count > TRACE_MAX)
        count = TRACE_MAX;
    pw_log("engine: %s trace, %u event(s):", label, count);
    while (i < count) {
        unsigned int width = 1, repeats = 1;

        for (unsigned int w = 2; w >= 1; w--) {
            unsigned int r = 1;

            while (i + (r + 1) * w <= count && ev[i + r * w] == ev[i] &&
                   (w == 1 || ev[i + r * w + 1] == ev[i + 1]))
                r++;
            if (r >= 3) {
                width = w;
                repeats = r;
                break;
            }
        }
        for (unsigned int k = 0; k < width; k++) {
            unsigned int v = ev[i + k];

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
               (cyc[count - 1] - cyc[0]) / calibrate());
}

static void log_trace(const StateHeader *h, int kind, const char *label)
{
    log_trace_raw(h->trace_ev[kind], h->trace_cyc[kind], h->trace_count[kind], label);
}

static void log_crumbs(const StateHeader *h, const char *who)
{
    pw_log("engine: %s: %u breadcrumb(s), last \"%s\"", who, h->crumb_count, stage_name(h->stage));
    for (unsigned int i = 0; i < h->crumb_count && i < CRUMB_MAX; i++)
        pw_log("engine:   %2u %s", h->crumb[i], stage_name(h->crumb[i]));
}

/*
 * Back in the SAVE-time thread after a LOAD rolled the kernel back. Nothing was written to the
 * card since the save, so the restored kernel and the card agree again: log lines go out right
 * away, and if anything below crashes the log shows how far it got. Writing makes the state
 * single-use (it is discarded at the end).
 */
static int finish_load(void)
{
    StateHeader *h = g_hdr;
    const pw_flash_report *f = &g_flash_report;
    unsigned int t0, scratch_crc;
    int ok_ram, ok_vram;
    u64 now;

    crumb(h, S_RB_THREAD);
    pw_log_set_deferred(0);
    pw_log("engine: ---- LOAD: back in the saved moment ----");
    pw_log("engine: load: froze %u thread(s) in %u ms (%u forced), GE %d", h->load_threads,
           h->load_freeze_ms, h->load_forced, h->load_ge);
    pw_log("engine: load: flash %d: drivers %d/%d, unassign %X/%X, exit %X/%X, init %X/%X, "
           "assign %X/%X",
           g_flash_result, f->found_lflash, f->found_flashfat, f->unassign0, f->unassign1,
           f->exit_lflash, f->exit_flashfat, f->init_lflash, f->init_flashfat, f->assign0,
           f->assign1);
    log_trace(h, TRACE_LOAD, "load (before rollback)");
    log_trace(h, TRACE_AFTER_ROLLBACK, "load (after rollback)");

    t0 = now_ms();
    pw_log("engine: load: restoring game RAM...");
    ok_ram = pw_stream_read(&g_arena, &h->user, (void *)USER_BASE, USER_SIZE) >= 0;
    crumb(h, S_RB_RAM);
    pw_log("engine: load: game RAM %s in %u ms", ok_ram ? "restored" : "DECOMPRESSION FAILED",
           now_ms() - t0);

    t0 = now_ms();
    pw_log("engine: load: restoring eDRAM...");
    ok_vram = pw_stream_read(&g_arena, &h->vram, (void *)VRAM_BASE, h->vram_size) >= 0;
    crumb(h, S_RB_VRAM);
    pw_log("engine: load: eDRAM %s in %u ms", ok_vram ? "restored" : "DECOMPRESSION FAILED",
           now_ms() - t0);

    /* Not restored (PSPFatSave does not either); compared to learn whether games rely on it. */
    scratch_crc = pw_crc32(0, (const void *)SCRATCH_BASE, SCRATCH_SIZE);
    pw_log("engine: load: scratchpad crc now %08X, at save %08X (left as is)", scratch_crc,
           h->scratch.crc32);

    sceKernelDcacheWritebackInvalidateAll();
    sceKernelIcacheInvalidateAll(); /* the game's code in RAM was replaced too */
    crumb(h, S_RB_CACHES);
    if (VERIFY) {
        unsigned int crc = pw_crc32(0, (const void *)USER_BASE, USER_SIZE);

        pw_log("engine: load: game RAM crc %08X, saved %08X: %s", crc, h->user.crc32,
               crc == h->user.crc32 ? "IDENTICAL" : "DIFFERENT");
    }

    g_restore_pending = 0;
    h->load_applying = 0;
    h->loads++;
    if (!ok_ram || !ok_vram) {
        pw_log("engine: load: restore failed; the game stays frozen (power the PSP off)");
        return -1;
    }

    pw_freeze_set watched = g_save_freeze; /* thawing clears the set */

    pw_log("engine: load: thawing %d game thread(s)...", watched.count);
    pw_thaw_game(&g_save_freeze);
    crumb(h, S_RB_THAWED);
    sceRtcGetCurrentTick(&now);
    crumb(h, S_LOAD_DONE);
    pw_log("engine: ---- LOAD complete: request -> game running %u ms (wall clock) ----",
           (unsigned int)((now - h->load_start_tick) / 1000));

    /* The card changed after the rollback: this state's view of it is stale now. */
    pw_engine_discard();

    if (VERIFY) /* if the game dies after the load, show for how long it lived */
        for (int tick = 1; tick <= 10; tick++) {
            int alive = 0;

            sceKernelDelayThread(500 * 1000);
            for (int i = 0; i < watched.count; i++) {
                SceKernelThreadInfo info;

                memset(&info, 0, sizeof(info));
                info.size = sizeof(info);
                alive += sceKernelReferThreadStatus(watched.ids[i], &info) >= 0;
            }
            pw_log("engine: after load +%d ms: %d/%d game thread(s) alive", tick * 500, alive,
                   watched.count);
        }
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
    crumb(h, S_SAVE_START);
    calibrate();

    pw_freeze_game(&g_save_freeze, FREEZE_TIMEOUT_MS);
    h->save_threads = (unsigned int)g_save_freeze.count;
    h->save_forced = (unsigned int)g_save_freeze.forced;
    h->save_freeze_ms = g_save_freeze.ms;
    crumb(h, S_SAVE_FROZEN);
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
    crumb(h, S_SAVE_PACKED);
    pw_freeze_log(&g_save_freeze);
    pw_log("engine: save: GE %d, packed in %u ms:", h->save_ge, h->save_pack_ms);
    log_stream("game", &h->user);
    log_stream("eDRAM", &h->vram);
    log_stream("scratch", &h->scratch);
    pw_log("engine:   P8      %6u KB at 0x%08X, copied with the kernel image", g_p8_size / 1024,
           g_p8_base);
    pw_log("engine: state record at %p (post-mortem if a load goes wrong)", (void *)h);

    /* From the kernel snapshot on, the Memory Stick must not change until the state is dropped. */
    pw_log_flush();
    pw_log_set_deferred(1);

    g_restore_pending = 0;
    g_alarm_set = g_resumed = g_captured = g_applying = 0;
    g_mode = MODE_SAVE;
    crumb(h, S_SAVE_SUSPEND);
    sceKernelDcacheWritebackAll();
    sceRtcGetCurrentTick(&rt0);
    ret = scePowerRequestSuspend();
    while (!g_resumed && polls++ < RESUME_POLLS)
        sceKernelDelayThread(5000);
    sceRtcGetCurrentTick(&rt1);
    g_mode = MODE_IDLE;

    if (g_restore_pending)
        return finish_load();

    h->save_suspend_ms = (unsigned int)((rt1 - rt0) / 1000);
    if (ret < 0 || !g_resumed || !g_captured) {
        pw_thaw_game(&g_save_freeze);
        if (!g_resumed)
            sceRtcSetAlarmTick(NULL);
        pw_log_set_deferred(0);
        pw_log("engine: save FAILED: suspend=0x%08X resumed=%d captured=%d", ret, g_resumed,
               g_captured);
        log_trace(h, TRACE_SAVE, "save");
        release_staging();
        return -1;
    }

    if (!g_keep_frozen) /* the frozen save+load test loads right away, game still frozen */
        pw_thaw_game(&g_save_freeze);
    h->valid = 1;
    crumb(h, S_SAVE_DONE);
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
    crumb(h, S_LOAD_START);
    pending_in_image = (volatile unsigned int *)(g_kernel_image +
                                                 ((unsigned int)&g_restore_pending - KERNEL_BASE));

    pw_freeze_game(&g_load_freeze, FREEZE_TIMEOUT_MS);
    h->load_threads = (unsigned int)g_load_freeze.count;
    h->load_forced = (unsigned int)g_load_freeze.forced;
    h->load_freeze_ms = g_load_freeze.ms;
    crumb(h, S_LOAD_FROZEN);
    h->load_ge = ge_wait_idle();

    /* The SAVE-time thread must learn it was brought back by a LOAD: flag it inside the image. */
    *pending_in_image = 1;
    memcpy(g_blob, pw_apply_handoff, (unsigned int)(pw_apply_handoff_end - pw_apply_handoff));
    h->trace_count[TRACE_LOAD] = 0;
    h->trace_count[TRACE_AFTER_ROLLBACK] = 0;
    h->load_applying = 1;
    sceRtcGetCurrentTick(&h->load_start_tick);
    crumb(h, S_LOAD_ARMED);

    g_alarm_set = g_resumed = g_captured = g_applying = 0;
    g_mode = MODE_LOAD;
    crumb(h, S_LOAD_SUSPEND);
    sceKernelDcacheWritebackAll(); /* the blob and the flag must be in DRAM */
    ret = scePowerRequestSuspend();
    while (!g_resumed && polls++ < RESUME_POLLS)
        sceKernelDelayThread(5000);

    /* Still here: PHASE0_0 never came (suspend refused or timed out). Undo and report. */
    g_mode = MODE_IDLE;
    h->load_applying = 0;
    *pending_in_image = 0;
    crumb(h, S_LOAD_FAILED);
    if (!g_resumed)
        sceRtcSetAlarmTick(NULL);
    pw_thaw_game(&g_load_freeze);
    pw_thaw_game(&g_save_freeze); /* still frozen after a frozen save+load test; no-op otherwise */
    pw_log("engine: LOAD FAILED: suspend=0x%08X resumed=%d applying=%d", ret, g_resumed,
           g_applying);
    log_trace(h, TRACE_LOAD, "load");
    return -1;
}

/*
 * Debug: the rollback mechanics without the world switch. During a real suspend, at PHASE0_0,
 * copy the live kernel and write it straight back through the apply blob, which then returns into
 * the current dispatcher. If the game carries on afterwards, the blob, the cache flushes and the
 * context restore work, and a failing load is about the saved world, not the mechanics.
 */
int pw_engine_selftest(void)
{
    StateHeader *h;
    pw_freeze_set frozen;
    int ret, polls = 0, ok;

    if (!g_registered)
        return -1;
    if (pw_engine_has_state())
        pw_engine_discard(); /* the test reuses the staging memory */
    pw_log("engine: ---- ROLLBACK SELF-TEST ----");
    if (check_environment() < 0 || ensure_staging() < 0)
        return -1;

    h = g_hdr;
    memset(h, 0, sizeof(*h));
    h->magic = STATE_MAGIC;
    crumb(h, S_ST_START);
    calibrate();

    pw_freeze_game(&frozen, FREEZE_TIMEOUT_MS);
    h->ge_selftest = ge_wait_idle();
    memcpy(g_blob, pw_apply_handoff, (unsigned int)(pw_apply_handoff_end - pw_apply_handoff));
    pw_log_flush();

    g_alarm_set = g_resumed = g_captured = g_applying = 0;
    g_mode = MODE_SELFTEST;
    sceKernelDcacheWritebackAll(); /* the blob must be in DRAM */
    ret = scePowerRequestSuspend();
    while (!g_resumed && polls++ < RESUME_POLLS)
        sceKernelDelayThread(5000);
    g_mode = MODE_IDLE;
    if (!g_resumed)
        sceRtcSetAlarmTick(NULL);
    pw_thaw_game(&frozen);

    ok = g_applying && g_resumed;
    pw_log("engine: self-test: froze %d thread(s), GE %d, suspend=0x%08X applied=%d resumed=%d: %s",
           frozen.count, h->ge_selftest, ret, g_applying, g_resumed,
           ok ? "ROLLBACK MECHANICS OK" : "FAILED");
    log_crumbs(h, "self-test");
    log_trace(h, TRACE_SAVE, "self-test");
    release_staging();
    return ok ? 0 : -1;
}

/*
 * Debug: save, keep the game frozen, load immediately. The saved world and the world being left
 * differ by as little as possible (only the save's own sleep/wake cycle).
 */
int pw_engine_save_load_frozen(void)
{
    int ret;

    pw_log("engine: ---- FROZEN SAVE + LOAD ----");
    g_keep_frozen = 1;
    ret = pw_engine_save(); /* returns PW_ENGINE_LOADED a second time, after the rollback */
    g_keep_frozen = 0;
    if (ret != PW_ENGINE_SAVED)
        return ret;
    return pw_engine_load();
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

/* Record layout of the first engine build (magic "PWS1", no breadcrumbs). */
typedef struct {
    unsigned int magic, valid, load_applying;
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
} StateHeaderV1;

#define STATE_MAGIC_V1 0x31535750u /* "PWS1" */

static void post_mortem_v1(StateHeaderV1 *old)
{
    pw_log("engine: post-mortem (v1 record at %p): valid=%u load_applying=%u loads=%u", (void *)old,
           old->valid, old->load_applying, old->loads);
    pw_log("engine: post-mortem: load froze %u thread(s) in %u ms, GE %d", old->load_threads,
           old->load_freeze_ms, old->load_ge);
    log_trace_raw(old->trace_ev[TRACE_SAVE], old->trace_cyc[TRACE_SAVE],
                  old->trace_count[TRACE_SAVE], "post-mortem save");
    log_trace_raw(old->trace_ev[TRACE_LOAD], old->trace_cyc[TRACE_LOAD],
                  old->trace_count[TRACE_LOAD], "post-mortem load (before rollback)");
    log_trace_raw(old->trace_ev[TRACE_AFTER_ROLLBACK], old->trace_cyc[TRACE_AFTER_ROLLBACK],
                  old->trace_count[TRACE_AFTER_ROLLBACK], "post-mortem load (after rollback)");
    old->magic = 0;
    sceKernelDcacheWritebackAll();
}

/*
 * The state record sits at the start of P11. If the previous game session died during a save or
 * load (or left a state behind), the record may have survived the trip back through the XMB:
 * report it once, then clear it.
 */
static void post_mortem(void)
{
    PspSysmemPartitionInfo p11;
    StateHeader *old;

    memset(&p11, 0, sizeof(p11));
    p11.size = sizeof(p11);
    if (sceKernelQueryMemoryPartitionInfo(PW_MEM_PARTITION_SPARE, &p11) < 0 ||
        p11.memsize < sizeof(StateHeader))
        return;
    old = (StateHeader *)((p11.startaddr + 63) & ~63u);
    if (old->magic == STATE_MAGIC_V1) {
        post_mortem_v1((StateHeaderV1 *)old);
        return;
    }
    if (old->magic != STATE_MAGIC || old->crumb_count == 0 || old->crumb_count > CRUMB_MAX)
        return;

    pw_log("engine: post-mortem: the previous session left a state record at %p", (void *)old);
    log_crumbs(old, "post-mortem");
    for (int k = 0; k < TRACE_KINDS; k++)
        if (old->trace_count[k] > TRACE_MAX)
            old->trace_count[k] = TRACE_MAX;
    log_trace(old, TRACE_SAVE, "post-mortem save");
    log_trace(old, TRACE_LOAD, "post-mortem load (before rollback)");
    log_trace(old, TRACE_AFTER_ROLLBACK, "post-mortem load (after rollback)");
    old->magic = 0;
    sceKernelDcacheWritebackAll();
}

int pw_engine_init(void)
{
    int ret;

    post_mortem();
    ret = sceKernelRegisterSysEventHandler(&g_handler);

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
