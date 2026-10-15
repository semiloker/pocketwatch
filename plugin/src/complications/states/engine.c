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
#define P8_PID       8
#define P8_SPACE     0x00100000u /* P8 (1 MB on the PSP-3000) is outside the kernel range */
#define ME_PID       11          /* P11: where the firmware saves the ME eDRAM across a suspend */
#define ME_SPACE     0x00400000u
#define UNCACHED(a)  ((a) | 0x20000000u) /* kseg0 address -> uncached kseg1 alias */

#define EV_POWER_LOCK_PHASE1 0x00000401
#define EV_FREEZE            0x00001000 /* the last event before _scePowerOffCommon */
#define EV_SUSPEND_PHASE0_0  0x00004000
#define EV_RESUME_PHASE0_0   0x00010000 /* the first resume event */
#define EV_RESUME_COMPLETED  0x00400000

/*
 * I/O lines that _scePowerOffCommon switches off before every suspend (uOFW powerSwitch.c): most
 * UART/SPI/GPIO pad enables and two GPIO outputs. A real wake gets them back from the IPL; a
 * touch-and-go suspend has no IPL, so the engine puts back what it saw at EV_FREEZE.
 */
#define REG_IO_ENABLE      ((volatile unsigned int *)0xBC100078)
#define REG_GPIO_IO_ENABLE ((volatile unsigned int *)0xBC10007C)
#define REG_GPIO_READ      ((volatile unsigned int *)0xBE240004)
#define REG_GPIO_SET       ((volatile unsigned int *)0xBE240008)
#define GPIO_OFF_BITS      0x02800000u

/*
 * Everything lives in one P9 block (24 MB at 0x8A000000). Not P11: on the PSP-2000/3000 the
 * firmware's me_wrapper saves the Media Engine's 4 MB eDRAM to P11 (sceKernelGetMEeDramSaveAddr,
 * event 0x4005, i.e. before PHASE0_0) during every suspend, whatever is allocated there. In E2
 * runs 1-4 that destroyed the apply blob, the state header and part of the arena.
 */
#define HEADER_AT    0x00000u /* StateHeader */
#define WORKSPACE_AT 0x08000u /* FastLZ workspace */
#define BLOB_AT      0x10000u /* apply blob */
#define BLOB_SPACE   0x01000u
#define IMAGE_AT     0x20000u /* kernel image, P8, ME eDRAM save (IMAGE_*_OFS) */
#define IMAGE_P8_OFS KERNEL_SIZE
#define IMAGE_ME_OFS (KERNEL_SIZE + P8_SPACE)
#define IMAGE_SPACE  (KERNEL_SIZE + P8_SPACE + ME_SPACE)
#define ARENA_AT     (IMAGE_AT + IMAGE_SPACE)
#define ARENA_MIN    0x00100000u
#define STATE_MAGIC  0x33535750u /* "PWS3": P9-only layout */

#define FREEZE_TIMEOUT_MS 500
#define GE_TIMEOUT_MS     100
#define RESUME_POLLS      2000 /* x 5 ms of active time; the sleep itself does not count */
/* RTC wake-up after the suspend request; it expires during the firmware's power-lock retries
   anyway, and an expired alarm still wakes the PSP at once (E1-E2). */
#define WAKE_DELAY_US 100000ull
#define TRACE_MAX     128

#ifdef PW_DEBUG
#define VERIFY 1 /* checksum game RAM on save and after load */
#else
#define VERIFY 0
#endif

enum { MODE_IDLE, MODE_SAVE, MODE_LOAD, MODE_SELFTEST, MODE_PLAIN /* suspend, nothing else */ };
enum { TRACE_SAVE, TRACE_LOAD, TRACE_AFTER_ROLLBACK, TRACE_KINDS };

/*
 * Breadcrumbs, kept in the P9 header: they survive the rollback and, with luck, a crash back to
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
    S_LOAD_FAILED,   /* suspend never reached PHASE0_0, or the guard refused the apply */
    S_LOAD_GUARD,    /* handler, PHASE0_0: blob or image damaged, apply skipped */
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
    S_ST_BACK,  /* first event after the blob returned */
    S_ST_GUARD, /* handler, PHASE0_0: blob damaged, apply skipped */
};

#define CRUMB_MAX  48
#define RAM_CHUNKS (USER_SIZE / PW_STREAM_CHUNK)

/* Wall-clock phase marks of one save or load (RTC, keeps counting through the sleep). */
enum {
    P_BEGIN,
    P_FREEZE,
    P_GE,
    P_PACK,
    P_SUMS,
    P_LOG,
    P_SUSPEND, /* request -> our thread runs again (sleep included) */
    P_THAW,
    P_VERIFY,
    P_RAM,
    P_VRAM,
    P_DIAG,
    P_KINDS
};
#define MARK_MAX 16

typedef struct {
    unsigned int n;
    unsigned int mhz_cpu, mhz_bus;
    unsigned long long t0;
    unsigned char id[MARK_MAX];
    unsigned int ms[MARK_MAX]; /* since t0 */
} Marks;

/* Guard results at PHASE0_0, checked before jumping to the blob (bit mask). */
#define GUARD_BLOB  1u /* the blob in P9 differs from the module's copy */
#define GUARD_IMAGE 2u /* the kernel image (+ P8) no longer has the save-time checksum */

/* Lives in P9, outside the kernel range: survives the rollback, readable by both sides. */
typedef struct {
    unsigned int magic;
    unsigned int valid;
    unsigned int load_applying; /* set by LOAD right before the rollback */
    pw_stream user, vram, scratch;
    unsigned int vram_size;
    unsigned int arena_used, arena_sum; /* bytes of region 0 in use and their checksum */
    unsigned int image_len, image_sum;  /* kernel image + P8 + ME, checksummed at capture */
    unsigned int image_sum_armed;       /* the same once LOAD has set its flag in the image */
    unsigned int guard;                 /* GUARD_* found at the last PHASE0_0 apply */
    unsigned int save_ic;               /* interrupt-controller state when PHASE0_0 came */
    unsigned int save_tg;               /* the save used a touch-and-go suspend (no sleep) */
    unsigned int io_snap[3];            /* I/O enables + GPIO levels at EV_FREEZE */
    unsigned int io_found[3];           /* the same at the first resume event */
    unsigned int io_restored;           /* touch-and-go resume: io_snap was put back */
    unsigned int ram_sum[RAM_CHUNKS];   /* debug: per-64 KB checksums of game RAM at save */
    Marks save_t, load_t;               /* phase timings (log_marks) */
    unsigned int flash_us;              /* flash driver rebuild at RESUME_COMPLETED */
    unsigned int save_threads, save_forced, save_freeze_ms, save_pack_ms, save_suspend_ms;
    int save_ge;
    unsigned int save_dmac_left, save_capture_us;
    unsigned int load_threads, load_forced, load_freeze_ms;
    int load_ge;
    unsigned long long load_start_tick;
    unsigned int loads;
    int ge_selftest;
    unsigned int stage; /* last breadcrumb */
    unsigned int crumb_count;
    unsigned int crumb[CRUMB_MAX];
    unsigned int crumb_us[CRUMB_MAX];
    unsigned int trace_count[TRACE_KINDS];
    unsigned int trace_ev[TRACE_KINDS][TRACE_MAX];
    unsigned int trace_us[TRACE_KINDS][TRACE_MAX];
} StateHeader;

_Static_assert(HEADER_AT + sizeof(StateHeader) <= WORKSPACE_AT, "state header overlaps workspace");
_Static_assert(WORKSPACE_AT + PW_STREAM_WORKSPACE <= BLOB_AT, "workspace overlaps the blob");
_Static_assert(BLOB_AT + BLOB_SPACE <= IMAGE_AT, "blob overlaps the kernel image");

/* Everything below is in kernel RAM, i.e. part of the snapshot. */
volatile unsigned int pw_handoff_ctx[16]; /* [0..12] pw_sysevent_shim, [13] blob run time */
#define CTX_APPLY_TICKS 13
static volatile unsigned int g_restore_pending; /* LOAD sets it to 1 inside the staged image */
static volatile int g_mode, g_alarm_set, g_resumed, g_captured, g_applying;
static StateHeader *volatile g_hdr;
static unsigned char *volatile g_kernel_image;
static void *volatile g_blob;
static pw_mem_block g_stage;
static pw_arena g_arena;
static pw_freeze_set g_save_freeze, g_load_freeze;
static unsigned int g_cycles_per_ms;
static int g_keep_frozen;              /* debug: pw_engine_save leaves the game frozen */
static pw_flash_report g_flash_report; /* filled at the save's RESUME_COMPLETED (flash.h) */
static volatile int g_flash_result;
static int g_registered;
static volatile int g_selftest_copy; /* self-test round: 0 = blob only, 1 = with the 4 MB copy */
static int g_fast;                   /* debug toggle: suspend touch-and-go (no sleep) */
static volatile int g_tg;            /* the suspend in progress was requested touch-and-go */
/* Partition 8 holds live data of something loaded at boot (343 KB used on the PSP-3000) but sits
   outside 0x88000000-0x88400000: it is captured and restored together with the kernel image. */
static unsigned int g_p8_base, g_p8_size;
/* The firmware's ME eDRAM save area (P11, written at event 0x4005 before PHASE0_0, read back to
   reboot the ME at resume). It belongs to the kernel's state like P8: a LOAD that kept the
   current ME image would restart the Media Engine out of step with the restored kernel. */
static unsigned int g_me_base, g_me_size;
static unsigned int g_ram_restored[RAM_CHUNKS], g_ram_now[RAM_CHUNKS]; /* debug, after a load */

extern int pw_sysevent_shim(int ev_id, char *ev_name, void *param, int *result);
extern char pw_apply_handoff[], pw_apply_handoff_end[];
int scePowerRequestSuspendTouchAndGo(void); /* core/stubs/scePower_driver.S */
int sceRtcSetAlarmTick(u64 *tick);          /* sceRtc_driver, not declared by PSPSDK */
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

/* Hardware system timer (us): unlike CP0 Count, its rate does not change during the suspend
   phases (E2 speed run 1: Count-based spans exceeded the wall clock). */
static unsigned int usec(void)
{
    return sceKernelGetSystemTimeLow();
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

/*
 * Mask interrupts for the kernel apply by clearing CP0 Status.IE, as PSPFatSave does. The blob
 * ends by reloading the dispatcher's saved Status, which unmasks them again. Never use
 * sceKernelCpuSuspendIntr here: it masks through the Allegrex interrupt controller (IC), which the
 * blob does not restore, so the resumed system would run on with every interrupt off (the cause
 * of the black screen and the drop to the XMB in E2 runs 1-4).
 */
static void mask_irq_status(void)
{
    unsigned int sr;

    __asm__ volatile("mfc0 %0, $12" : "=r"(sr));
    __asm__ volatile("mtc0 %0, $12\n\tnop\n\tnop\n\tnop\n\tnop" : : "r"(sr & ~1u) : "memory");
}

/* What the guard checks right before the blob runs: GUARD_* bits, 0 when safe to jump. */
static unsigned int apply_guard(int check_image)
{
    unsigned int len = (unsigned int)(pw_apply_handoff_end - pw_apply_handoff);
    unsigned int bad = 0;

    if (memcmp(g_blob, pw_apply_handoff, len) != 0)
        bad |= GUARD_BLOB;
    if (check_image && pw_sum32(g_kernel_image, g_hdr->image_len) != g_hdr->image_sum_armed)
        bad |= GUARD_IMAGE;
    return bad;
}

typedef void (*apply_fn)(const void *src, void *dst, unsigned int len, const volatile void *ctx);

/* The blob in P9, called through the cached segment: the caller staged it with stage_blob. */
static apply_fn blob(void)
{
    return (apply_fn)g_blob;
}

/* Copy the apply blob to P9 and make it fetchable: D-cache to RAM, no stale I-cache lines. */
static void stage_blob(void)
{
    memcpy(g_blob, pw_apply_handoff, (unsigned int)(pw_apply_handoff_end - pw_apply_handoff));
    sceKernelDcacheWritebackAll();
    sceKernelIcacheInvalidateAll();
}

static int request_suspend(void)
{
    return g_tg ? scePowerRequestSuspendTouchAndGo() : scePowerRequestSuspend();
}

/* Memory only: safe in the sysevent handler. */
static void crumb(StateHeader *h, unsigned int stage)
{
    unsigned int n = h->crumb_count;

    h->stage = stage;
    if (n < CRUMB_MAX) {
        h->crumb[n] = stage;
        h->crumb_us[n] = usec();
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
        return "load:failed(no rollback)";
    case S_LOAD_GUARD:
        return "load:guard-refused-apply";
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
    case S_ST_GUARD:
        return "selftest:guard-refused-apply";
    default:
        return "?";
    }
}

static void trace(StateHeader *h, int kind, unsigned int ev)
{
    unsigned int n = h->trace_count[kind];

    if (n < TRACE_MAX) {
        h->trace_ev[kind][n] = ev;
        h->trace_us[kind][n] = usec();
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

    if (ev_id == EV_FREEZE) {
        h->io_snap[0] = *REG_IO_ENABLE;
        h->io_snap[1] = *REG_GPIO_IO_ENABLE;
        h->io_snap[2] = *REG_GPIO_READ;
    } else if (ev_id == EV_RESUME_PHASE0_0) {
        /* A rolled-back save resumes the way the save suspended: its mode is in the header. */
        int tg = g_mode == MODE_SAVE ? (int)h->save_tg : g_tg;

        h->io_found[0] = *REG_IO_ENABLE;
        h->io_found[1] = *REG_GPIO_IO_ENABLE;
        h->io_found[2] = *REG_GPIO_READ;
        if (tg && h->io_snap[0]) {
            *REG_IO_ENABLE |= h->io_snap[0];
            *REG_GPIO_IO_ENABLE |= h->io_snap[1];
            if (h->io_snap[2] & GPIO_OFF_BITS)
                *REG_GPIO_SET = h->io_snap[2] & GPIO_OFF_BITS;
            h->io_restored = 1;
        }
    }

    /* No alarm when nothing will sleep: a touch-and-go suspend, or a load that rolls back into a
       touch-and-go save (a load into a normal save does sleep, inside the save's suspend). */
    if (ev_id == EV_POWER_LOCK_PHASE1 && !g_alarm_set &&
        !(g_tg && (g_mode != MODE_LOAD || h->save_tg))) {
        u64 tick;

        sceRtcGetCurrentTick(&tick);
        tick += WAKE_DELAY_US;
        sceRtcSetAlarmTick(&tick);
        g_alarm_set = 1;
    } else if (ev_id == EV_SUSPEND_PHASE0_0) {
        if (g_mode == MODE_SAVE && !g_captured) {
            unsigned int t0 = usec(), intr;

            h->save_dmac_left = dmac_drain();
            /* No interrupt may change the kernel mid-copy. A matched pair, so the IC mask is
               undone here; the state found is logged (is PHASE0_0 dispatched with IRQs on?). */
            intr = sceKernelCpuSuspendIntr();
            memcpy(g_kernel_image, (const void *)KERNEL_BASE, KERNEL_SIZE);
            if (g_p8_size)
                memcpy(g_kernel_image + IMAGE_P8_OFS, (const void *)g_p8_base, g_p8_size);
            if (g_me_size) /* written by the ME, not the CPU: read past the D-cache */
                memcpy(g_kernel_image + IMAGE_ME_OFS, (const void *)UNCACHED(g_me_base), g_me_size);
            sceKernelCpuResumeIntr(intr);
            h->save_ic = intr;
            h->image_len = IMAGE_SPACE;
            h->image_sum = pw_sum32(g_kernel_image, h->image_len);
            sceKernelDcacheWritebackAll(); /* the image must be in DRAM before power-down */
            h->save_capture_us = usec() - t0;
            g_captured = 1;
            crumb(h, S_SAVE_CAPTURED);
        } else if (g_mode == MODE_LOAD && !g_applying) {
            g_applying = 1;
            h->guard = apply_guard(1);
            if (h->guard) { /* jumping now would kill the system: stay in this world, report */
                crumb(h, S_LOAD_GUARD);
                return 0;
            }
            crumb(h, S_LOAD_APPLY); /* written back by the blob's cache flush */
            dmac_drain();
            mask_irq_status(); /* unmasked by the blob, with the save-time Status */
            /* Outside the kernel range: put back before the kernel. The blob's D-cache flush
               writes both to RAM before the ME is rebooted from its image at resume. */
            if (g_p8_size)
                memcpy((void *)g_p8_base, g_kernel_image + IMAGE_P8_OFS, g_p8_size);
            if (g_me_size)
                memcpy((void *)g_me_base, g_kernel_image + IMAGE_ME_OFS, g_me_size);
            blob()(g_kernel_image, (void *)KERNEL_BASE, KERNEL_SIZE, pw_handoff_ctx);
            /* not reached: execution continues in the SAVE's suspend */
        } else if (g_mode == MODE_SELFTEST && !g_applying) {
            g_applying = 1;
            h->guard = apply_guard(0);
            if (h->guard) {
                crumb(h, S_ST_GUARD);
                return 0;
            }
            crumb(h, S_ST_APPLY);
            dmac_drain();
            mask_irq_status();
            /* Snapshot the live kernel and put it straight back: the rollback minus the world
             * switch. Round 0 calls the blob with nothing to copy. */
            if (g_selftest_copy)
                memcpy(g_kernel_image, (const void *)KERNEL_BASE, KERNEL_SIZE);
            blob()(g_kernel_image, (void *)KERNEL_BASE, g_selftest_copy ? KERNEL_SIZE : 0,
                   pw_handoff_ctx);
            /* not reached: the blob returns into this dispatch */
        }
    } else if (ev_id == EV_RESUME_COMPLETED) {
        sceRtcSetAlarmTick(NULL); /* never wake the next natural sleep early */
        if (g_mode == MODE_SAVE) {
            /* Rebuild the flash drivers before anyone else writes flash (flash.h). After a
               rollback their state is the save's and the NAND may have changed since: remount. */
            unsigned int t0 = usec();

            crumb(h, g_restore_pending ? S_RB_RESUME : S_SAVE_RESUMED);
            g_flash_result = pw_flash_reinit(&g_flash_report, g_restore_pending);
            h->flash_us = usec() - t0;
            if (g_restore_pending)
                crumb(h, S_RB_FLASH);
        }
        g_resumed = 1;
    }
    return 0;
}

static unsigned int calibrate(void)
{
    if (g_cycles_per_ms == 0) { /* CP0 Count ticks per ms, against the system timer */
        unsigned int c0 = cycles(), t0 = usec(), dt;

        sceKernelDelayThread(50 * 1000);
        dt = usec() - t0;
        g_cycles_per_ms = dt ? (unsigned int)((unsigned long long)(cycles() - c0) * 1000 / dt) : 0;
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

    PspSysmemPartitionInfo me;

    memset(&me, 0, sizeof(me));
    me.size = sizeof(me);
    if (sceKernelQueryMemoryPartitionInfo(ME_PID, &me) >= 0 && me.memsize == ME_SPACE) {
        g_me_base = me.startaddr;
        g_me_size = me.memsize;
    } else {
        g_me_base = 0;
        g_me_size = 0;
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
    pw_mem_free(&g_stage);
}

/* One P9 block: header, workspace, blob, kernel image + P8, arena (see HEADER_AT...). */
static int ensure_staging(void)
{
    unsigned int size, blob_len = (unsigned int)(pw_apply_handoff_end - pw_apply_handoff);
    unsigned char *base;
    int ret;

    if (g_hdr != NULL)
        return 0;
    if (blob_len > BLOB_SPACE)
        return -1;
    size = pw_mem_max_free(PW_MEM_PARTITION_EXTRA) & ~0xFFFFu;
    if (size < ARENA_AT + ARENA_MIN) {
        pw_log("engine: not enough spare RAM (P9 %u KB)", size / 1024);
        return -1;
    }
    if ((ret = pw_mem_alloc_in(&g_stage, PW_MEM_PARTITION_EXTRA, "PwState", size)) < 0) {
        pw_log("engine: staging alloc failed 0x%08X", ret);
        return -1;
    }

    base = g_stage.ptr;
    g_hdr = (StateHeader *)(base + HEADER_AT);
    pw_stream_set_workspace(base + WORKSPACE_AT);
    g_blob = base + BLOB_AT;
    g_kernel_image = base + IMAGE_AT;
    g_arena.base[0] = base + ARENA_AT;
    g_arena.size[0] = size - ARENA_AT;
    g_arena.base[1] = NULL; /* P11 would be overwritten by the ME eDRAM save at every suspend */
    g_arena.size[1] = 0;
    return 0;
}

/* Bytes of the arena a state occupies (one region in use, see ensure_staging). */
static unsigned int arena_used(const pw_arena_pos *cursor)
{
    return cursor->region == 0 ? cursor->offset : g_arena.size[0];
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
static void log_trace_raw(const unsigned int *ev, const unsigned int *us, unsigned int count,
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
        pw_log("engine: %s trace spans %u ms (system timer)", label,
               (us[count - 1] - us[0]) / 1000);
}

static void log_trace(const StateHeader *h, int kind, const char *label)
{
    log_trace_raw(h->trace_ev[kind], h->trace_us[kind], h->trace_count[kind], label);
}

static void log_crumbs(const StateHeader *h, const char *who)
{
    pw_log("engine: %s: %u breadcrumb(s), last \"%s\"", who, h->crumb_count, stage_name(h->stage));
    for (unsigned int i = 0; i < h->crumb_count && i < CRUMB_MAX; i++)
        pw_log("engine:   %2u %s", h->crumb[i], stage_name(h->crumb[i]));
}

static void mark_begin(Marks *m)
{
    memset(m, 0, sizeof(*m));
    sceRtcGetCurrentTick(&m->t0);
    m->mhz_cpu = (unsigned int)scePowerGetCpuClockFrequency();
    m->mhz_bus = (unsigned int)scePowerGetBusClockFrequency();
}

/* End of a phase: the time since the previous mark is charged to id. */
static void mark(Marks *m, int id)
{
    u64 now;

    sceRtcGetCurrentTick(&now);
    if (m->n < MARK_MAX) {
        m->id[m->n] = (unsigned char)id;
        m->ms[m->n] = (unsigned int)((now - m->t0) / 1000);
        m->n++;
    }
}

/* Milliseconds charged to phase id (0 if it was not reached). */
static unsigned int mark_span(const Marks *m, int id)
{
    for (unsigned int i = 0; i < m->n && i < MARK_MAX; i++)
        if (m->id[i] == id)
            return m->ms[i] - (i > 0 ? m->ms[i - 1] : 0);
    return 0;
}

static void log_marks(const Marks *m, const char *who)
{
    static const char *const names[P_KINDS] = {
        "begin",   "freeze", "ge",     "pack", "sums", "log",
        "suspend", "thaw",   "verify", "ram",  "vram", "diag",
    };
    char line[200];
    int len = 0;
    unsigned int prev = 0;

    for (unsigned int i = 0; i < m->n && i < MARK_MAX && len < 160; i++) {
        len += stbsp_snprintf(line + len, sizeof(line) - (unsigned int)len, " %s %u",
                              m->id[i] < P_KINDS ? names[m->id[i]] : "?", m->ms[i] - prev);
        prev = m->ms[i];
    }
    line[len] = '\0';
    pw_log("engine: timing %s (CPU %u MHz, bus %u MHz):%s = %u ms", who, m->mhz_cpu, m->mhz_bus,
           line, prev);
}

/* Time between the first ev_a and the first later ev_b of a trace, in ms; -1 if absent. */
static int trace_span(const StateHeader *h, int kind, unsigned int ev_a, unsigned int ev_b)
{
    unsigned int n = h->trace_count[kind] < TRACE_MAX ? h->trace_count[kind] : TRACE_MAX;

    for (unsigned int a = 0; a < n; a++)
        if (h->trace_ev[kind][a] == ev_a)
            for (unsigned int b = a + 1; b < n; b++)
                if (h->trace_ev[kind][b] == ev_b)
                    return (int)((h->trace_us[kind][b] - h->trace_us[kind][a]) / 1000);
    return -1;
}

static unsigned int trace_count_of(const StateHeader *h, int kind, unsigned int ev)
{
    unsigned int n = h->trace_count[kind] < TRACE_MAX ? h->trace_count[kind] : TRACE_MAX, c = 0;

    for (unsigned int i = 0; i < n; i++)
        c += h->trace_ev[kind][i] == ev;
    return c;
}

/* Where a suspend's wall-clock time went: spans from the event trace, the rest is the sleep. */
static void log_suspend_split(const StateHeader *h, int pre, int post, const char *who,
                              unsigned int wall_ms)
{
    int queries = trace_span(h, pre, 0x100, 0x401), retries = trace_span(h, pre, 0x401, 0x1000);
    int phases = trace_span(h, pre, 0x1000, 0x4000),
        resume = trace_span(h, post, 0x10000, 0x400000);
    int cpu = (queries > 0 ? queries : 0) + (retries > 0 ? retries : 0) +
              (phases > 0 ? phases : 0) + (resume > 0 ? resume : 0);

    pw_log("engine: %s suspend %u ms: queries %d, power-lock retries %d (0x402 x%u), suspend "
           "phases %d, resume phases %d, flash rebuild %u; sleep+wake+rest ~%d ms",
           who, wall_ms, queries, retries, trace_count_of(h, pre, 0x402), phases, resume,
           h->flash_us / 1000, (int)wall_ms - cpu);
}

static void log_io(const StateHeader *h, const char *who)
{
    pw_log("engine: %s: I/O enables %08X/%08X, GPIO %08X at suspend; %08X/%08X, GPIO %08X at "
           "resume%s",
           who, h->io_snap[0], h->io_snap[1], h->io_snap[2], h->io_found[0], h->io_found[1],
           h->io_found[2], h->io_restored ? " -> put back (touch-and-go)" : "");
}

static void log_flash(const char *who)
{
    const pw_flash_report *f = &g_flash_report;

    pw_log(
        "engine: %s: flash drivers re-initialised: %s (lflash %d exit %X init %X, flashfat %d exit "
        "%X init %X)",
        who, g_flash_result == 0 ? "ok" : "FAILED", f->found_lflash, f->exit_lflash, f->init_lflash,
        f->found_flashfat, f->exit_flashfat, f->init_flashfat);
    if (f->remount)
        pw_log("engine: %s: flash0/flash1 remounted: unassign %X/%X, assign %X/%X", who,
               f->unassign0, f->unassign1, f->assign0, f->assign1);
}

/* Debug: per-64 KB checksums of game RAM. */
static void ram_sums(unsigned int *out)
{
    for (unsigned int i = 0; i < RAM_CHUNKS; i++)
        out[i] = pw_sum32((const void *)(USER_BASE + i * PW_STREAM_CHUNK), PW_STREAM_CHUNK);
}

/* Debug: log which parts of game RAM differ from ref (runs of 64 KB chunks, user addresses). */
static unsigned int log_ram_diff(const char *what, const unsigned int *ref, unsigned int *now)
{
    unsigned int diff = 0, runs = 0;

    ram_sums(now);
    for (unsigned int i = 0; i < RAM_CHUNKS;) {
        unsigned int j = i;

        if (now[i] == ref[i]) {
            i++;
            continue;
        }
        while (j < RAM_CHUNKS && now[j] != ref[j])
            j++;
        diff += j - i;
        if (runs++ < 16)
            pw_log("engine:   %s: 0x%08X-0x%08X", what,
                   (USER_BASE & 0x1FFFFFFFu) + i * PW_STREAM_CHUNK,
                   (USER_BASE & 0x1FFFFFFFu) + j * PW_STREAM_CHUNK - 1);
        i = j;
    }
    pw_log("engine: load: %s: %u of %u 64 KB chunk(s) differ", what, diff, RAM_CHUNKS);
    return diff;
}

/* Debug: is flash0 readable after the rollback and the driver rebuild? */
static int flash0_probe(void)
{
    char buf[64];
    SceUID fd = sceIoOpen("flash0:/vsh/etc/version.txt", PSP_O_RDONLY, 0);
    int n;

    if (fd < 0)
        return fd;
    n = sceIoRead(fd, buf, sizeof(buf));
    sceIoClose(fd);
    return n;
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
    unsigned int t0, scratch_crc;
    int ok_ram, ok_vram;
    u64 now;

    crumb(h, S_RB_THREAD);
    mark(&h->load_t, P_SUSPEND);
    /* Lines stay in RAM until the game runs again: card writes here would extend the freeze. */
    pw_log("engine: ---- LOAD: back in the saved moment ----");
    pw_log("engine: load: blob ran %u us", pw_handoff_ctx[CTX_APPLY_TICKS] * 1000 / calibrate());
    pw_log("engine: load: froze %u thread(s) in %u ms (%u forced), GE %d", h->load_threads,
           h->load_freeze_ms, h->load_forced, h->load_ge);
    log_flash("load");
    log_trace(h, TRACE_LOAD, "load (before rollback)");
    log_trace(h, TRACE_AFTER_ROLLBACK, "load (after rollback)");

    t0 = now_ms();
    pw_log("engine: load: restoring game RAM...");
    ok_ram = pw_stream_read(&g_arena, &h->user, (void *)USER_BASE, USER_SIZE) >= 0;
    crumb(h, S_RB_RAM);
    mark(&h->load_t, P_RAM);
    pw_log("engine: load: game RAM %s in %u ms", ok_ram ? "restored" : "DECOMPRESSION FAILED",
           now_ms() - t0);
    if (VERIFY && ok_ram) /* differences here: RAM moved while the save was being packed */
        log_ram_diff("restored vs save", h->ram_sum, g_ram_restored);

    t0 = now_ms();
    pw_log("engine: load: restoring eDRAM...");
    ok_vram = pw_stream_read(&g_arena, &h->vram, (void *)VRAM_BASE, h->vram_size) >= 0;
    crumb(h, S_RB_VRAM);
    mark(&h->load_t, P_VRAM);
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
        pw_log("engine: load: reading flash0...");
        pw_log("engine: load: flash0 read %d", flash0_probe());
        if (ok_ram) /* differences here: written by someone else while the game is frozen */
            log_ram_diff("changed since restore", g_ram_restored, g_ram_now);
    }

    mark(&h->load_t, P_DIAG);
    g_restore_pending = 0;
    h->load_applying = 0;
    h->loads++;
    if (!ok_ram || !ok_vram) {
        pw_log("engine: load: restore failed; the game stays frozen (power the PSP off)");
        pw_log_set_deferred(0);
        pw_log_flush();
        return -1;
    }

    pw_freeze_set watched = g_save_freeze; /* thawing clears the set */

    pw_log("engine: load: thawing %d game thread(s)...", watched.count);
    pw_thaw_game(&g_save_freeze);
    crumb(h, S_RB_THAWED);
    mark(&h->load_t, P_THAW);
    sceRtcGetCurrentTick(&now);
    crumb(h, S_LOAD_DONE);
    pw_log("engine: ---- LOAD complete: request -> game running %u ms (wall clock) ----",
           (unsigned int)((now - h->load_start_tick) / 1000));
    /* The save's own timing lines were in the RAM log, which the rollback rewound. */
    log_marks(&h->save_t, h->save_tg ? "save (touch-and-go)" : "save");
    log_suspend_split(h, TRACE_SAVE, TRACE_SAVE, "save", h->save_suspend_ms);
    log_io(h, "load");
    log_marks(&h->load_t, "load");
    log_suspend_split(h, TRACE_LOAD, TRACE_AFTER_ROLLBACK, "load",
                      mark_span(&h->load_t, P_SUSPEND));

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
    pw_log("engine: CP0 Count runs at %u kHz", calibrate());
    mark_begin(&h->save_t);

    pw_freeze_game(&g_save_freeze, FREEZE_TIMEOUT_MS);
    mark(&h->save_t, P_FREEZE);
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
    mark(&h->save_t, P_GE);

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
    mark(&h->save_t, P_PACK);
    h->arena_used = arena_used(&cursor);
    h->arena_sum = pw_sum32(g_arena.base[0], h->arena_used);
    if (VERIFY)
        ram_sums(h->ram_sum);
    mark(&h->save_t, P_SUMS);
    crumb(h, S_SAVE_PACKED);
    pw_freeze_log(&g_save_freeze);
    pw_log("engine: save: GE %d, packed in %u ms:", h->save_ge, h->save_pack_ms);
    log_stream("game", &h->user);
    log_stream("eDRAM", &h->vram);
    log_stream("scratch", &h->scratch);
    pw_log("engine:   ME save %6u KB at 0x%08X, copied with the kernel image", g_me_size / 1024,
           g_me_base);
    pw_log("engine:   P8      %6u KB at 0x%08X, copied with the kernel image", g_p8_size / 1024,
           g_p8_base);
    pw_log("engine: state record at %p (post-mortem if a load goes wrong)", (void *)h);

    /* From the kernel snapshot on, the Memory Stick must not change until the state is dropped. */
    pw_log_flush();
    pw_log_set_deferred(1);
    mark(&h->save_t, P_LOG);

    g_restore_pending = 0;
    g_alarm_set = g_resumed = g_captured = g_applying = 0;
    g_tg = g_fast;
    h->save_tg = (unsigned int)g_tg;
    g_mode = MODE_SAVE;
    crumb(h, S_SAVE_SUSPEND);
    sceKernelDcacheWritebackAll();
    sceRtcGetCurrentTick(&rt0);
    ret = request_suspend();
    while (!g_resumed && polls++ < RESUME_POLLS)
        sceKernelDelayThread(5000);
    sceRtcGetCurrentTick(&rt1);
    g_mode = MODE_IDLE;

    if (g_restore_pending)
        return finish_load();
    mark(&h->save_t, P_SUSPEND);

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
    mark(&h->save_t, P_THAW);
    h->valid = 1;
    crumb(h, S_SAVE_DONE);
    pw_log("engine: save OK: suspend %u ms (wall clock), kernel copy %u ms, dmac_left=0x%X, "
           "IC at PHASE0_0 0x%X",
           h->save_suspend_ms, h->save_capture_us / 1000, h->save_dmac_left, h->save_ic);
    pw_log("engine: save: image %u KB sum %08X, arena %u KB sum %08X", h->image_len / 1024,
           h->image_sum, h->arena_used / 1024, h->arena_sum);
    log_flash("save");
    log_io(h, "save");
    log_trace(h, TRACE_SAVE, "save");
    log_marks(&h->save_t, h->save_tg ? "save (touch-and-go)" : "save");
    log_suspend_split(h, TRACE_SAVE, TRACE_SAVE, "save", h->save_suspend_ms);
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
    mark_begin(&h->load_t);

    /* Last point where a load can be refused without consequences. Without the flash driver
       rebuild a rollback corrupts flash1 (the system settings): never load then. */
    if (!pw_flash_available()) {
        pw_log("engine: LOAD refused: the flash drivers cannot be found to rebuild them");
        return -1;
    }
    unsigned int arena_now = pw_sum32(g_arena.base[0], h->arena_used);
    unsigned int image_now = pw_sum32(g_kernel_image, h->image_len);

    if (arena_now != h->arena_sum || image_now != h->image_sum) {
        pw_log("engine: LOAD refused, the state was overwritten in RAM: arena %08X/%08X, "
               "kernel image %08X/%08X (now/saved)",
               arena_now, h->arena_sum, image_now, h->image_sum);
        pw_engine_discard();
        return -1;
    }
    pending_in_image = (volatile unsigned int *)(g_kernel_image +
                                                 ((unsigned int)&g_restore_pending - KERNEL_BASE));
    mark(&h->load_t, P_VERIFY);

    pw_freeze_game(&g_load_freeze, FREEZE_TIMEOUT_MS);
    mark(&h->load_t, P_FREEZE);
    h->load_threads = (unsigned int)g_load_freeze.count;
    h->load_forced = (unsigned int)g_load_freeze.forced;
    h->load_freeze_ms = g_load_freeze.ms;
    crumb(h, S_LOAD_FROZEN);
    h->load_ge = ge_wait_idle();
    mark(&h->load_t, P_GE);

    /* The SAVE-time thread must learn it was brought back by a LOAD: flag it inside the image. */
    *pending_in_image = 1;
    h->image_sum_armed = pw_sum32(g_kernel_image, h->image_len);
    mark(&h->load_t, P_SUMS);
    h->guard = 0;
    h->trace_count[TRACE_LOAD] = 0;
    h->trace_count[TRACE_AFTER_ROLLBACK] = 0;
    h->load_applying = 1;
    sceRtcGetCurrentTick(&h->load_start_tick);
    crumb(h, S_LOAD_ARMED);

    g_alarm_set = g_resumed = g_captured = g_applying = 0;
    g_tg = g_fast;
    h->io_restored = 0;
    g_mode = MODE_LOAD;
    crumb(h, S_LOAD_SUSPEND);
    stage_blob(); /* its D-cache write-back also puts the flag in the image into DRAM */
    ret = request_suspend();
    while (!g_resumed && polls++ < RESUME_POLLS)
        sceKernelDelayThread(5000);

    /* Still here: PHASE0_0 never came (suspend refused or timed out), or the guard refused to
       jump. Undo and report. */
    g_mode = MODE_IDLE;
    h->load_applying = 0;
    *pending_in_image = 0;
    crumb(h, S_LOAD_FAILED);
    if (!g_resumed)
        sceRtcSetAlarmTick(NULL);
    pw_thaw_game(&g_load_freeze);
    pw_thaw_game(&g_save_freeze); /* still frozen after a frozen save+load test; no-op otherwise */
    pw_log("engine: LOAD FAILED: suspend=0x%08X resumed=%d applying=%d guard=%X%s%s", ret,
           g_resumed, g_applying, h->guard, h->guard & GUARD_BLOB ? " (blob damaged)" : "",
           h->guard & GUARD_IMAGE ? " (kernel image damaged)" : "");
    log_crumbs(h, "load");
    log_trace(h, TRACE_LOAD, "load");
    if (h->guard)
        pw_engine_discard(); /* something overwrites P9 during a suspend: do not trust it */
    return -1;
}

/*
 * One self-test round: a real suspend in which, at PHASE0_0, the blob runs and returns into the
 * current dispatcher. copy = 0: nothing to copy (jump, cache flushes, context and interrupt
 * restore only). copy = 1: the live kernel is snapshotted and written straight back, i.e. the
 * rollback minus the world switch.
 */
static int selftest_round(int copy)
{
    StateHeader *h = g_hdr;
    pw_freeze_set frozen;
    int ret, polls = 0, ok;

    memset(h, 0, sizeof(*h));
    h->magic = STATE_MAGIC;
    crumb(h, S_ST_START);
    pw_handoff_ctx[CTX_APPLY_TICKS] = 0;
    h->guard = 0;

    pw_freeze_game(&frozen, FREEZE_TIMEOUT_MS);
    h->ge_selftest = ge_wait_idle();
    pw_log("engine: self-test round %d (%s): suspending...", copy,
           copy ? "blob + 4 MB kernel copy" : "blob without copy");
    pw_log_flush(); /* if the round kills the system, the card still says which one */

    g_alarm_set = g_resumed = g_captured = g_applying = 0;
    g_tg = g_fast;
    g_selftest_copy = copy;
    g_mode = MODE_SELFTEST;
    stage_blob();
    ret = request_suspend();
    while (!g_resumed && polls++ < RESUME_POLLS)
        sceKernelDelayThread(5000);
    g_mode = MODE_IDLE;
    if (!g_resumed)
        sceRtcSetAlarmTick(NULL);
    pw_thaw_game(&frozen);

    ok = g_applying && g_resumed && !h->guard;
    pw_log("engine: self-test round %d: froze %d thread(s), GE %d, suspend=0x%08X applied=%d "
           "resumed=%d guard=%X, blob %u us: %s",
           copy, frozen.count, h->ge_selftest, ret, g_applying, g_resumed, h->guard,
           pw_handoff_ctx[CTX_APPLY_TICKS] * 1000 / calibrate(), ok ? "OK" : "FAILED");
    log_crumbs(h, "self-test");
    log_trace(h, TRACE_SAVE, "self-test");
    pw_log_flush();
    return ok ? 0 : -1;
}

/*
 * Debug: the rollback mechanics without the world switch, in two suspends: the blob alone, then
 * the blob with the kernel copy. If the game carries on afterwards, the blob, the cache flushes
 * and the context restore work, and a failing load is about the saved world, not the mechanics.
 */
int pw_engine_selftest(void)
{
    int ret;

    if (!g_registered)
        return -1;
    if (pw_engine_has_state())
        pw_engine_discard(); /* the test reuses the staging memory */
    pw_log("engine: ---- ROLLBACK SELF-TEST ----");
    if (check_environment() < 0 || ensure_staging() < 0)
        return -1;
    calibrate();

    ret = selftest_round(0);
    if (ret == 0)
        ret = selftest_round(1);
    pw_log("engine: ---- ROLLBACK SELF-TEST %s ----", ret == 0 ? "PASSED" : "FAILED");
    release_staging();
    return ret;
}

void pw_engine_set_fast(int on)
{
    g_fast = on != 0;
    pw_log("engine: fast mode (touch-and-go suspend, no sleep) %s", g_fast ? "ON" : "OFF");
}

int pw_engine_fast(void)
{
    return g_fast;
}

/*
 * Debug: one touch-and-go suspend and nothing else (no freeze, no capture), to check that the
 * system and the game come back whole without a real sleep: screen, sound, buttons, card, battery.
 */
int pw_engine_tg_test(void)
{
    StateHeader *h;
    int ret, polls = 0;

    if (!g_registered)
        return -1;
    if (pw_engine_has_state())
        pw_engine_discard();
    pw_log("engine: ---- TOUCH-AND-GO SUSPEND TEST ----");
    if (ensure_staging() < 0)
        return -1;
    h = g_hdr;
    memset(h, 0, sizeof(*h));
    h->magic = STATE_MAGIC;
    mark_begin(&h->save_t);
    pw_log_flush();

    g_alarm_set = g_resumed = g_captured = g_applying = 0;
    g_tg = 1;
    g_mode = MODE_PLAIN;
    ret = scePowerRequestSuspendTouchAndGo();
    while (!g_resumed && polls++ < RESUME_POLLS)
        sceKernelDelayThread(5000);
    g_mode = MODE_IDLE;
    mark(&h->save_t, P_SUSPEND);

    pw_log("engine: touch-and-go: request=0x%08X resumed=%d", ret, g_resumed);
    log_io(h, "touch-and-go");
    log_trace(h, TRACE_SAVE, "touch-and-go");
    log_marks(&h->save_t, "touch-and-go");
    log_suspend_split(h, TRACE_SAVE, TRACE_SAVE, "touch-and-go", mark_span(&h->save_t, P_SUSPEND));
    release_staging();
    return g_resumed ? 0 : -1;
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

/*
 * The state record sits at the start of P9. If the previous game session died during a save or
 * load (or left a state behind), the record may have survived the trip back through the XMB:
 * report it once, then clear it.
 */
static void post_mortem(void)
{
    PspSysmemPartitionInfo p9;
    StateHeader *old;

    memset(&p9, 0, sizeof(p9));
    p9.size = sizeof(p9);
    if (sceKernelQueryMemoryPartitionInfo(PW_MEM_PARTITION_EXTRA, &p9) < 0 || p9.memsize < IMAGE_AT)
        return;
    old = (StateHeader *)(((p9.startaddr + 63) & ~63u) + HEADER_AT);
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
