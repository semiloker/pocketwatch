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
 * 0xBFC00200-0xBFC0021F, in the 4 KB reset-vector SRAM, hold the seeds the IPL leaves for memlmd.
 * Every suspend backs the SRAM up with those 8 words zeroed; a real wake gets fresh seeds from the
 * IPL and _scePowerResumePoint keeps them, but touch-and-go writes the backup back as it is. The
 * next reboot (quitting the game) then cannot decrypt reboot.bin - memlmd_F26A33C3(0, 0xBFC00200)
 * rejects all-zero seeds - and loadexec stops with a black screen, which looked like the PSP
 * sleeping or switching off (speed runs 3-6; uOFW powerSwitch.c, loadcore.c, memlmd_02.c).
 * The engine saves them before every suspend and puts them back at the first resume event of a
 * touch-and-go.
 */
#define REG_RESET_SEEDS ((volatile unsigned int *)0xBFC00200)
#define SEED_WORDS      8

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
#define VERIFY 1 /* cheap checks after the game runs again (thread liveness after a load) */
#else
#define VERIFY 0
#endif
/* Heavy diagnostics inside the frozen window: CRC of the raw data, per-64 KB RAM sums at save and
   their comparison after a load, a flash0 read. ~1.5 s per save/load; on only to chase a restore
   problem (E2 runs 5-7 used them). */
#define DIAG 0

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

/*
 * A touch-and-go suspend zeroes the resume info (ScePowerResumeInfo, uOFW power_kernel.h) that a
 * real wake gets from the IPL, and at 0x10008 sceSysconResume rebuilds the syscon driver's view of
 * the switches and device power from it: with zeroes it would believe HOLD on, the Memory Stick
 * and Wi-Fi powered off, no battery... (E2 speed run 3: the PSP went to sleep when the game was
 * quit). The engine reads the real state before suspending and writes it back at 0x10000.
 */
typedef struct {
    int ok;
    int hold, wlan_switch, hp, ms_power, wlan_power, hr_power;
    int supply, batt_status, batt_remain, batt_full;
} LiveStatus;

#define RI_WORDS       17 /* the resume info up to and including baryonStatus2 (+64) */
#define RI_BARYON_ST   24 /* u8; bit 1 Wi-Fi power, bit 2 HR power (sceSysconResume) */
#define RI_BARYON_KEY  32 /* u32; bit 13 !HOLD, 14 !Wi-Fi switch, 15 headphones, 21 MS power */
#define RI_SUPPLY      36
#define RI_BATT_STATUS 40
#define RI_BATT_REMAIN 44
#define RI_BATT_FULL   48

/* The resume info of the last real wake in this boot: the bits no getter reports come from it. */
static unsigned int g_real_ri[RI_WORDS];
static int g_real_ri_valid;

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
    unsigned int guard;                 /* GUARD_* found at the last PHASE0_0 apply */
    unsigned int save_ic;               /* interrupt-controller state when PHASE0_0 came */
    unsigned int save_tg;               /* the save used a touch-and-go suspend (no sleep) */
    unsigned int io_snap[3];            /* I/O enables + GPIO levels at EV_FREEZE */
    unsigned int io_found[3];           /* the same at the first resume event */
    unsigned int io_restored;           /* touch-and-go resume: io_snap was put back */
    LiveStatus live;                    /* switches, power and battery right before the suspend */
    unsigned int ri_found[RI_WORDS];    /* resume info as the first resume event found it */
    unsigned int ri_filled;             /* touch-and-go resume: resume info filled (1 + real) */
    unsigned int real_ri[RI_WORDS];     /* g_real_ri, kept here because a load rewinds it */
    unsigned int real_ri_valid;
    unsigned int seeds[SEED_WORDS];       /* the IPL's seeds before the suspend */
    unsigned int seeds_found[SEED_WORDS]; /* the same at the first resume event */
    unsigned int seeds_ok, seeds_restored;
    unsigned int ram_sum[RAM_CHUNKS]; /* debug: per-64 KB checksums of game RAM at save */
    Marks save_t, load_t;             /* phase timings (log_marks) */
    unsigned int flash_us;            /* flash driver rebuild at RESUME_COMPLETED */
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
    unsigned int caller[PW_ENGINE_USER_WORDS]; /* the caller's words: they survive a rollback */
    /* For a load in another boot (E3): where the save-time kernel kept these, and where
       everything outside the kernel image was. */
    unsigned int at_pending, at_ctx;
    pw_engine_env env;
} StateHeader;

_Static_assert(HEADER_AT + sizeof(StateHeader) <= WORKSPACE_AT, "state header overlaps workspace");
_Static_assert(WORKSPACE_AT + PW_STREAM_WORKSPACE <= BLOB_AT, "workspace overlaps the blob");
_Static_assert(BLOB_AT + BLOB_SPACE <= IMAGE_AT, "blob overlaps the kernel image");

/* Everything below is in kernel RAM, i.e. part of the snapshot. */
volatile unsigned int pw_handoff_ctx[16]; /* [0..12] pw_sysevent_shim, [13] blob run time */
#define CTX_APPLY_TICKS 13
static volatile unsigned int g_restore_pending; /* LOAD sets it to 1 inside the staged image */
static volatile unsigned int *volatile g_pending_in_image; /* where, in the image (P9) */
static volatile int g_mode, g_alarm_set, g_resumed, g_captured, g_applying;
static StateHeader *volatile g_hdr;
static unsigned char *volatile g_kernel_image;
static void *volatile g_blob;
static pw_mem_block g_stage;
static pw_arena g_arena;
static pw_freeze_set g_save_freeze, g_load_freeze;
static unsigned int g_cycles_per_ms;
static int g_keep_frozen; /* debug: pw_engine_save leaves the game frozen */
/* The save's caller froze the game itself (the menu) and thaws it: the engine neither freezes
   nor thaws. Set before the kernel capture, so a load's rollback brings back the save's value. */
static int g_caller_freeze;
static pw_flash_report g_flash_report;         /* filled at the save's RESUME_COMPLETED (flash.h) */
static volatile int g_busy;                    /* a save or load is running (one at a time) */
static volatile unsigned int g_last_resume_ms; /* system time of the last RESUME_COMPLETED */
static void (*g_presuspend)(void);             /* called right before every suspend request */
static int (*g_load_hook)(unsigned int *, unsigned int); /* after a rollback, before card I/O */
static unsigned int g_env_extra[PW_ENGINE_ENV_EXTRA];    /* the caller's layout facts (assets) */
static volatile int g_flash_result, g_flash_skipped;
static int g_registered;
static volatile int g_selftest_copy; /* self-test round: 0 = blob only, 1 = with the 4 MB copy */
static int g_fast = 1;               /* suspend touch-and-go (no sleep); debug toggle */
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
/* core/stubs/sceSyscon_driver.S (uOFW syscon.c signatures) */
signed char sceSysconGetHoldSwitch(void);
signed char sceSysconGetWlanSwitch(void);
signed char sceSysconGetHPConnect(void);
signed char sceSysconGetMsPowerCtrl(void);
signed char sceSysconGetWlanPowerCtrl(void);
signed char sceSysconGetHRPowerCtrl(void);
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
    if (check_image && pw_sum32(g_kernel_image, g_hdr->image_len) != g_hdr->image_sum)
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

/*
 * The syscon driver's cached view of the switches and device power: plain variable reads, no
 * syscon traffic. (Speed run 4: issuing real syscon commands here - power supply and battery
 * reads - hung the save with the game frozen, and the PSP slept on quitting.) Battery and supply
 * values come only from a real wake's resume info, when there was one this boot.
 */
static void capture_live(LiveStatus *l)
{
    memset(l, 0, sizeof(*l));
    l->hold = sceSysconGetHoldSwitch();
    l->wlan_switch = sceSysconGetWlanSwitch();
    l->hp = sceSysconGetHPConnect();
    l->ms_power = sceSysconGetMsPowerCtrl();
    l->wlan_power = sceSysconGetWlanPowerCtrl();
    l->hr_power = sceSysconGetHRPowerCtrl();
    if (g_real_ri_valid) {
        l->supply = (int)g_real_ri[RI_SUPPLY / 4];
        l->batt_status = (int)g_real_ri[RI_BATT_STATUS / 4];
        l->batt_remain = (int)g_real_ri[RI_BATT_REMAIN / 4];
        l->batt_full = (int)g_real_ri[RI_BATT_FULL / 4];
    }
    l->ok = 1;
}

/* Handler, first resume event of a touch-and-go suspend: make the zeroed resume info tell the
   truth, so that sceSysconResume and the battery driver keep what they knew before. */
static void fill_resume_info(unsigned int *ri, const LiveStatus *l)
{
    unsigned char *b = (unsigned char *)ri;
    unsigned int key = g_real_ri_valid ? g_real_ri[RI_BARYON_KEY / 4] : 0xFFFFu; /* as Adrenaline */
    unsigned int st = g_real_ri_valid ? (g_real_ri[RI_BARYON_ST / 4] & 0xFF) : 0;

    st = (st & ~6u) | (l->wlan_power ? 2u : 0) | (l->hr_power ? 4u : 0);
    key &= ~((1u << 13) | (1u << 14) | (1u << 15) | (1u << 21));
    key |= (l->hold ? 0 : 1u << 13) | (l->wlan_switch ? 0 : 1u << 14) | (l->hp ? 1u << 15 : 0) |
           (l->ms_power ? 1u << 21 : 0);
    b[RI_BARYON_ST] = (unsigned char)st;
    ri[RI_BARYON_KEY / 4] = key;
    if (g_real_ri_valid) { /* else: "no battery" until the battery driver polls again */
        ri[RI_SUPPLY / 4] = (unsigned int)l->supply;
        ri[RI_BATT_STATUS / 4] = (unsigned int)l->batt_status;
        ri[RI_BATT_REMAIN / 4] = (unsigned int)l->batt_remain;
        ri[RI_BATT_FULL / 4] = (unsigned int)l->batt_full;
        b[64] = (unsigned char)g_real_ri[64 / 4]; /* baryonStatus2 */
    }
}

static int seeds_present(const unsigned int *w)
{
    unsigned int any = 0;

    for (int i = 0; i < SEED_WORDS; i++)
        any |= w[i];
    return any != 0;
}

/* Thread context, right before every suspend request (the header survives a load's rollback). */
static void capture_before_suspend(StateHeader *h)
{
    capture_live(&h->live);
    for (int i = 0; i < SEED_WORDS; i++)
        h->seeds[i] = REG_RESET_SEEDS[i];
    h->seeds_ok = (unsigned int)seeds_present(h->seeds);
    h->seeds_restored = 0;
}

/*
 * Fast mode suspends touch-and-go only once this boot has seen a real wake: its resume info gives
 * the battery and switch bits that no getter reports. So the first save of a session sleeps for
 * real (slow); the following saves and loads do not.
 */
static int use_touch_and_go(void)
{
    unsigned int now[SEED_WORDS];

    for (int i = 0; i < SEED_WORDS; i++)
        now[i] = REG_RESET_SEEDS[i];
    /* Without seeds to put back afterwards a touch-and-go would make the next reboot hang. */
    return g_fast && g_real_ri_valid && seeds_present(now);
}

static int request_suspend(void)
{
    if (!g_tg)
        return scePowerRequestSuspend();
    return scePowerRequestSuspendTouchAndGo();
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
    (void)result;
    if (ev_id == EV_RESUME_COMPLETED)
        g_last_resume_ms = sceKernelGetSystemTimeLow() / 1000;
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

        for (int i = 0; i < SEED_WORDS; i++)
            h->seeds_found[i] = REG_RESET_SEEDS[i];
        if (tg && h->seeds_ok) {
            for (int i = 0; i < SEED_WORDS; i++)
                REG_RESET_SEEDS[i] = h->seeds[i];
            h->seeds_restored = 1;
        }

        h->io_found[0] = *REG_IO_ENABLE;
        h->io_found[1] = *REG_GPIO_IO_ENABLE;
        h->io_found[2] = *REG_GPIO_READ;

        /* Resume payload: pResumeInfo at +4 (sceSysconResume reads it there at 0x10008). */
        unsigned int *ri = param ? *(unsigned int **)((char *)param + 4) : NULL;

        if (ri != NULL && ((unsigned int)ri & 0xF0000003u) == 0x80000000u) {
            for (int i = 0; i < RI_WORDS; i++)
                h->ri_found[i] = ri[i];
            if (!tg) { /* a real wake: keep what the IPL reported */
                for (int i = 0; i < RI_WORDS; i++)
                    g_real_ri[i] = h->real_ri[i] = ri[i];
                g_real_ri_valid = 1;
                h->real_ri_valid = 1;
            } else if (h->live.ok) {
                fill_resume_info(ri, &h->live);
                h->ri_filled = 1 + g_real_ri_valid;
            }
        }
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
            /* The save-time thread must learn it was brought back by a LOAD: flag it inside the
               image now that the guard has checked it (the blob copies through the D-cache). */
            *g_pending_in_image = 1;
            crumb(h, S_LOAD_APPLY); /* written back by the blob's cache flush */
            dmac_drain();
            mask_irq_status(); /* unmasked by the blob, with the save-time Status */
            /* Outside the kernel range: put back before the kernel. The blob's D-cache flush
               writes both to RAM before the ME is rebooted from its image at resume. */
            if (g_p8_size)
                memcpy((void *)g_p8_base, g_kernel_image + IMAGE_P8_OFS, g_p8_size);
            if (g_me_size)
                memcpy((void *)g_me_base, g_kernel_image + IMAGE_ME_OFS, g_me_size);
            /* the save-time context: in another boot our module (and its context) sat elsewhere */
            blob()(g_kernel_image, (void *)KERNEL_BASE, KERNEL_SIZE,
                   h->at_ctx ? (const volatile void *)h->at_ctx : pw_handoff_ctx);
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
            /* A touch-and-go save never lost power: its flash state is still the NAND's. */
            g_flash_skipped = !g_restore_pending && h->save_tg;
            if (!g_flash_skipped)
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
    if (g_hdr != NULL && g_mode == MODE_IDLE)
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

/* The events whose handlers (and the power code after them) took longest, slowest first. */
static void log_slowest(const StateHeader *h, int kind, const char *label)
{
    unsigned int n = h->trace_count[kind] < TRACE_MAX ? h->trace_count[kind] : TRACE_MAX;
    unsigned int used[6] = {0}, picked = 0;
    char line[200];
    int len = 0;

    for (unsigned int k = 0; k < 6; k++) {
        unsigned int best = 0, best_i = 0;

        for (unsigned int i = 0; i + 1 < n; i++) {
            unsigned int gap = h->trace_us[kind][i + 1] - h->trace_us[kind][i], taken = 0;

            for (unsigned int u = 0; u < picked; u++)
                taken |= used[u] == i + 1;
            if (!taken && gap > best) {
                best = gap;
                best_i = i;
            }
        }
        if (best < 1000)
            break;
        used[picked++] = best_i + 1;
        len += stbsp_snprintf(line + len, sizeof(line) - (unsigned int)len, " %X:%u",
                              h->trace_ev[kind][best_i], best / 1000);
    }
    line[len] = 0;
    pw_log("engine: %s slowest events (id:ms):%s", label, line);
}

static void log_trace(const StateHeader *h, int kind, const char *label)
{
    log_trace_raw(h->trace_ev[kind], h->trace_us[kind], h->trace_count[kind], label);
    log_slowest(h, kind, label);
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

static void log_seeds(const char *who, const char *what, const unsigned int *w)
{
    pw_log("engine: %s: IPL seeds %s: %08X %08X %08X %08X %08X %08X %08X %08X", who, what, w[0],
           w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
}

static void log_resume_info(const StateHeader *h, const char *who)
{
    const LiveStatus *l = &h->live;
    const unsigned int *r = h->ri_found;

    pw_log("engine: %s: live before suspend: hold %d wlan-sw %d hp %d ms-pow %d wlan-pow %d hr-pow "
           "%d supply %X batt %X %d/%d mAh",
           who, l->hold, l->wlan_switch, l->hp, l->ms_power, l->wlan_power, l->hr_power, l->supply,
           l->batt_status, l->batt_remain, l->batt_full);
    pw_log("engine: %s: resume info found: size %u wake %X st %02X clock %u key %08X supply %X "
           "batt %X %u/%d st2 %02X%s",
           who, r[0], r[1], r[6] & 0xFF, r[7], r[8], r[9], r[10], r[11], (int)r[12], r[16] & 0xFF,
           h->ri_filled == 2   ? " -> filled (live + real wake)"
           : h->ri_filled == 1 ? " -> filled (live only)"
                               : "");
    log_seeds(who, "before suspend", h->seeds);
    log_seeds(who, h->seeds_restored ? "at resume (put back)" : "at resume", h->seeds_found);
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

    if (g_flash_skipped) {
        pw_log("engine: %s: flash drivers left alone (touch-and-go: no power cycle)", who);
        return;
    }
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
    if (h->real_ri_valid && !g_real_ri_valid) {
        for (int i = 0; i < RI_WORDS; i++)
            g_real_ri[i] = h->real_ri[i];
        g_real_ri_valid = 1;
    }
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
    if (DIAG && ok_ram) /* differences here: RAM moved while the save was being packed */
        log_ram_diff("restored vs save", h->ram_sum, g_ram_restored);

    t0 = now_ms();
    pw_log("engine: load: restoring eDRAM...");
    ok_vram = pw_stream_read(&g_arena, &h->vram, (void *)VRAM_BASE, h->vram_size) >= 0;
    crumb(h, S_RB_VRAM);
    mark(&h->load_t, P_VRAM);
    pw_log("engine: load: eDRAM %s in %u ms", ok_vram ? "restored" : "DECOMPRESSION FAILED",
           now_ms() - t0);

    /* Not restored: the power code's resume block brings back the save's scratchpad (run 7). */
    if (DIAG) {
        scratch_crc = pw_crc32(0, (const void *)SCRATCH_BASE, SCRATCH_SIZE);
        pw_log("engine: load: scratchpad crc now %08X, at save %08X (left as is)", scratch_crc,
               h->scratch.crc32);
    }

    sceKernelDcacheWritebackInvalidateAll();
    sceKernelIcacheInvalidateAll(); /* the game's code in RAM was replaced too */
    crumb(h, S_RB_CACHES);
    if (DIAG) {
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

    if (!g_caller_freeze) {
        pw_log("engine: load: thawing %d game thread(s)...", watched.count);
        pw_thaw_game(&g_save_freeze);
    }
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
    log_resume_info(h, "load");
    log_marks(&h->load_t, "load");
    log_suspend_split(h, TRACE_LOAD, TRACE_AFTER_ROLLBACK, "load",
                      mark_span(&h->load_t, P_SUSPEND));

    g_last_resume_ms = sceKernelGetSystemTimeLow() / 1000;
    if (g_load_hook) {
        int (*hook)(unsigned int *, unsigned int) = g_load_hook;

        g_load_hook = NULL;
        if (hook(h->caller, PW_ENGINE_USER_WORDS) < 0) {
            pw_log("engine: load: the card check failed: no Memory Stick writes until reboot");
            pw_log_set_card_blocked(1);
        }
    }

    /* The card changed after the rollback: this state's view of it is stale now. */
    pw_engine_discard();

    if (VERIFY && !g_caller_freeze) /* if the game dies after the load: how long it lived */
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

static int save_impl(const pw_freeze_set *held)
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
    h->at_pending = (unsigned int)&g_restore_pending;
    h->at_ctx = (unsigned int)pw_handoff_ctx;
    pw_engine_env_now(&h->env);
    if (g_real_ri_valid) {
        for (int i = 0; i < RI_WORDS; i++)
            h->real_ri[i] = g_real_ri[i];
        h->real_ri_valid = 1;
    }
    crumb(h, S_SAVE_START);
    pw_log("engine: CP0 Count runs at %u kHz", calibrate());
    /* Release builds keep the log in RAM from here (a card write would extend the freeze); debug
       builds write it until the suspend request, so a hang shows where it happened. After the
       kernel snapshot a card write would make the saved FAT view stale: RAM only, always. */
    pw_log_flush();
    if (!VERIFY)
        pw_log_set_deferred(1);
    mark_begin(&h->save_t);

    g_caller_freeze = held != NULL;
    if (held)
        g_save_freeze = *held;
    else
        pw_freeze_game(&g_save_freeze, FREEZE_TIMEOUT_MS);
    mark(&h->save_t, P_FREEZE);
    h->save_threads = (unsigned int)g_save_freeze.count;
    h->save_forced = (unsigned int)g_save_freeze.forced;
    h->save_freeze_ms = g_save_freeze.ms;
    crumb(h, S_SAVE_FROZEN);
    if (g_save_freeze.count == 0) {
        pw_log_set_deferred(0);
        pw_log("engine: no game threads to freeze");
        release_staging();
        return -1;
    }
    h->save_ge = ge_wait_idle();
    h->vram_size = sceGeEdramGetSize();
    mark(&h->save_t, P_GE);

    t0 = now_ms();
    if (pw_stream_write(&g_arena, &cursor, (const void *)USER_BASE, USER_SIZE, DIAG, &h->user) <
            0 ||
        pw_stream_write(&g_arena, &cursor, (const void *)VRAM_BASE, h->vram_size, DIAG, &h->vram) <
            0 ||
        pw_stream_write(&g_arena, &cursor, (const void *)SCRATCH_BASE, SCRATCH_SIZE, DIAG,
                        &h->scratch) < 0) {
        if (!g_caller_freeze)
            pw_thaw_game(&g_save_freeze);
        g_save_freeze.count = 0;
        pw_log_set_deferred(0);
        pw_log("engine: the state does not fit in spare RAM");
        release_staging();
        return -1;
    }
    h->save_pack_ms = now_ms() - t0;
    mark(&h->save_t, P_PACK);
    h->arena_used = arena_used(&cursor);
    h->arena_sum = pw_sum32(g_arena.base[0], h->arena_used);
    if (DIAG)
        ram_sums(h->ram_sum);
    mark(&h->save_t, P_SUMS);
    crumb(h, S_SAVE_PACKED);
    pw_freeze_log(&g_save_freeze);
    pw_log("engine: save: GE %d, packed in %u ms:", h->save_ge, h->save_pack_ms);
    log_stream("game", &h->user);
    log_stream("eDRAM", &h->vram);
    log_stream("scratch", &h->scratch);
    pw_log("engine:   %u FILL chunk(s) (one repeated word)",
           h->user.fill_chunks + h->vram.fill_chunks + h->scratch.fill_chunks);
    pw_log("engine:   ME save %6u KB at 0x%08X, copied with the kernel image", g_me_size / 1024,
           g_me_base);
    pw_log("engine:   P8      %6u KB at 0x%08X, copied with the kernel image", g_p8_size / 1024,
           g_p8_base);
    pw_log("engine: state record at %p (post-mortem if a load goes wrong)", (void *)h);

    g_restore_pending = 0;
    g_alarm_set = g_resumed = g_captured = g_applying = 0;
    g_tg = use_touch_and_go();
    h->save_tg = (unsigned int)g_tg;
    pw_log("engine: save: requesting a %s suspend", g_tg ? "touch-and-go" : "real");
    pw_log_set_deferred(1);
    if (g_presuspend)
        g_presuspend();
    g_mode = MODE_SAVE;
    crumb(h, S_SAVE_SUSPEND);
    sceKernelDcacheWritebackAll();
    sceRtcGetCurrentTick(&rt0);
    capture_before_suspend(h);
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
        if (!g_caller_freeze)
            pw_thaw_game(&g_save_freeze);
        g_save_freeze.count = 0;
        if (!g_resumed)
            sceRtcSetAlarmTick(NULL);
        pw_log_set_deferred(0);
        pw_log("engine: save FAILED: suspend=0x%08X resumed=%d captured=%d", ret, g_resumed,
               g_captured);
        log_trace(h, TRACE_SAVE, "save");
        release_staging();
        return -1;
    }

    if (g_caller_freeze)
        g_save_freeze.count = 0; /* the caller thaws (after a load too: see finish_load) */
    else if (!g_keep_frozen)     /* the frozen save+load test loads right away, game still frozen */
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
    log_resume_info(h, "save");
    log_trace(h, TRACE_SAVE, "save");
    log_marks(&h->save_t, h->save_tg ? "save (touch-and-go)" : "save");
    log_suspend_split(h, TRACE_SAVE, TRACE_SAVE, "save", h->save_suspend_ms);
    /* T1 (2026-10-16): after a rollback the Memory Stick driver sees the card as it is, so card
       writes while a state is held are safe; the log goes to the card again. */
    pw_log_set_deferred(0);
    pw_log_flush();
    return PW_ENGINE_SAVED;
}

static int load_impl(const pw_freeze_set *held)
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
    pending_in_image =
        (volatile unsigned int *)(g_kernel_image +
                                  ((h->at_pending ? h->at_pending
                                                  : (unsigned int)&g_restore_pending) -
                                   KERNEL_BASE));
    mark(&h->load_t, P_VERIFY);

    if (held) {
        g_load_freeze = *held;
        g_load_freeze.count = 0; /* nothing of ours to thaw */
        h->load_threads = (unsigned int)held->count;
    } else {
        pw_freeze_game(&g_load_freeze, FREEZE_TIMEOUT_MS);
        if (g_load_freeze.card_busy) { /* thawed: the game runs and keeps the card (ADR-032) */
            pw_log("engine: LOAD refused: the game keeps the card, it could not be frozen");
            return -1;
        }
    }
    mark(&h->load_t, P_FREEZE);
    if (!held)
        h->load_threads = (unsigned int)g_load_freeze.count;
    h->load_forced = (unsigned int)g_load_freeze.forced;
    h->load_freeze_ms = g_load_freeze.ms;
    crumb(h, S_LOAD_FROZEN);
    h->load_ge = ge_wait_idle();
    mark(&h->load_t, P_GE);

    g_pending_in_image = pending_in_image;
    h->guard = 0;
    h->trace_count[TRACE_LOAD] = 0;
    h->trace_count[TRACE_AFTER_ROLLBACK] = 0;
    h->load_applying = 1;
    sceRtcGetCurrentTick(&h->load_start_tick);
    crumb(h, S_LOAD_ARMED);

    g_alarm_set = g_resumed = g_captured = g_applying = 0;
    g_tg = use_touch_and_go();
    h->io_restored = 0;
    h->ri_filled = 0;
    if (g_presuspend)
        g_presuspend();
    g_mode = MODE_LOAD;
    crumb(h, S_LOAD_SUSPEND);
    stage_blob(); /* its D-cache write-back also puts the flag in the image into DRAM */
    capture_before_suspend(h);
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
    pw_thaw_game(&g_load_freeze); /* empty when the caller holds the freeze */
    if (!held)
        pw_thaw_game(&g_save_freeze); /* still frozen after a frozen save+load test; else no-op */
    pw_log("engine: LOAD FAILED: suspend=0x%08X resumed=%d applying=%d guard=%X%s%s", ret,
           g_resumed, g_applying, h->guard, h->guard & GUARD_BLOB ? " (blob damaged)" : "",
           h->guard & GUARD_IMAGE ? " (kernel image damaged)" : "");
    log_crumbs(h, "load");
    log_trace(h, TRACE_LOAD, "load");
    if (h->guard)
        pw_engine_discard(); /* something overwrites P9 during a suspend: do not trust it */
    return -1;
}

/* The busy flag is part of the snapshot: a load comes back inside save_guarded with it set, and
   the return path clears it. A load that succeeds never returns from load_guarded. */
static int save_guarded(const pw_freeze_set *held)
{
    int ret;

    if (g_busy)
        return PW_ENGINE_BUSY;
    g_busy = 1;
    ret = save_impl(held);
    g_busy = 0;
    return ret;
}

static int load_guarded(const pw_freeze_set *held)
{
    int ret;

    if (g_busy)
        return PW_ENGINE_BUSY;
    g_busy = 1;
    ret = load_impl(held);
    g_busy = 0;
    return ret;
}

int pw_engine_save(void)
{
    return save_guarded(NULL);
}

int pw_engine_save_held(const pw_freeze_set *held)
{
    return held ? save_guarded(held) : -1;
}

int pw_engine_load(void)
{
    return load_guarded(NULL);
}

int pw_engine_load_held(const pw_freeze_set *held)
{
    return held ? load_guarded(held) : -1;
}

int pw_engine_busy(void)
{
    return g_busy;
}

unsigned int pw_engine_last_resume_ms(void)
{
    return g_last_resume_ms;
}

void pw_engine_set_presuspend(void (*fn)(void))
{
    g_presuspend = fn;
}

void pw_engine_set_load_hook(int (*fn)(unsigned int *user, unsigned int words))
{
    g_load_hook = fn;
}

unsigned int *pw_engine_user_area(void)
{
    return g_hdr != NULL ? g_hdr->caller : NULL;
}

void pw_engine_prepare_exit(void)
{
    unsigned int now[SEED_WORDS];

    for (int i = 0; i < SEED_WORDS; i++)
        now[i] = REG_RESET_SEEDS[i];
    if (!seeds_present(now) && g_hdr != NULL && seeds_present(g_hdr->seeds)) {
        for (int i = 0; i < SEED_WORDS; i++)
            REG_RESET_SEEDS[i] = g_hdr->seeds[i];
        pw_log("engine: exit: the IPL seeds were missing, put back from the state record");
    }
    for (int i = 0; i < SEED_WORDS; i++)
        now[i] = REG_RESET_SEEDS[i];
    log_seeds("exit", seeds_present(now) ? "present" : "MISSING (the reboot may hang)", now);
    if (g_hdr != NULL)
        pw_engine_discard();
    else
        pw_log_flush();
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
    if (frozen.card_busy) { /* thawed: the game runs and keeps the card (ADR-032) */
        pw_log("engine: self-test round %d refused: the game keeps the card", copy);
        return -1;
    }
    h->ge_selftest = ge_wait_idle();
    pw_log("engine: self-test round %d (%s): suspending...", copy,
           copy ? "blob + 4 MB kernel copy" : "blob without copy");
    pw_log_flush(); /* if the round kills the system, the card still says which one */

    g_alarm_set = g_resumed = g_captured = g_applying = 0;
    g_tg = use_touch_and_go();
    g_selftest_copy = copy;
    g_mode = MODE_SELFTEST;
    stage_blob();
    capture_before_suspend(h);
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
    pw_log("engine: fast mode (touch-and-go suspend, no sleep) %s%s", g_fast ? "ON" : "OFF",
           g_fast && !g_real_ri_valid ? " - the next suspend still sleeps for real once" : "");
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
    capture_before_suspend(h);
    ret = scePowerRequestSuspendTouchAndGo();
    while (!g_resumed && polls++ < RESUME_POLLS)
        sceKernelDelayThread(5000);
    g_mode = MODE_IDLE;
    mark(&h->save_t, P_SUSPEND);

    pw_log("engine: touch-and-go: request=0x%08X resumed=%d", ret, g_resumed);
    log_io(h, "touch-and-go");
    log_resume_info(h, "touch-and-go");
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

void pw_engine_set_env_extra(int index, unsigned int value)
{
    if (index >= 0 && index < PW_ENGINE_ENV_EXTRA)
        g_env_extra[index] = value;
}

void pw_engine_env_now(pw_engine_env *e)
{
    memset(e, 0, sizeof(*e));
    e->layout = STATE_MAGIC;
    /* The kernel image holds this very module: a state loads only into the build that made it. */
    e->build =
        pw_crc32(pw_crc32(0, (const unsigned char *)PW_ENGINE_BUILD, sizeof(PW_ENGINE_BUILD) - 1),
                 (const unsigned char *)PW_VERSION, sizeof(PW_VERSION) - 1);
    e->header_size = sizeof(StateHeader);
    e->model = (unsigned int)sceKernelGetModel();
    e->devkit = (unsigned int)sceKernelDevkitVersion();
    e->stage = (unsigned int)g_stage.ptr;
    e->p8_base = g_p8_base;
    e->p8_size = g_p8_size;
    e->me_base = g_me_base;
    e->me_size = g_me_size;
    for (int i = 0; i < PW_ENGINE_ENV_EXTRA; i++)
        e->extra[i] = g_env_extra[i];
}

int pw_engine_env_for_load(pw_engine_env *e)
{
    if (g_busy)
        return PW_ENGINE_BUSY;
    if (check_environment() < 0 || ensure_staging() < 0)
        return -1;
    pw_engine_env_now(e);
    return 0;
}

void pw_engine_env_log(const char *who, const pw_engine_env *saved, const pw_engine_env *now)
{
    static const char *const names[] = {
        "layout",  "build",   "header", "model",   "devkit", "P9",       "P8 base", "P8 size",
        "ME base", "ME size", "assets", "prx crc", "sfo",    "playtime", "battery", "extra5"};
    const unsigned int *a = (const unsigned int *)saved, *b = (const unsigned int *)now;
    int n = (int)(sizeof(*saved) / sizeof(unsigned int)), any = 0;

    for (int i = 0; i < n; i++)
        if (a[i] != b[i]) {
            pw_log("%s: %s saved %08X, now %08X", who,
                   i < (int)(sizeof(names) / sizeof(names[0])) ? names[i] : "?", a[i], b[i]);
            any = 1;
        }
    if (!any)
        pw_log("%s: the setup is the same", who);
}

int pw_engine_export(pw_state_parts *p)
{
    StateHeader *h = g_hdr;

    if (!pw_engine_has_state() || g_busy)
        return -1;
    p->header = h;
    p->header_len = sizeof(*h);
    p->image = g_kernel_image;
    p->image_len = h->image_len;
    p->arena = g_arena.base[0];
    p->arena_len = h->arena_used;
    return 0;
}

int pw_engine_import(pw_state_parts *p)
{
    if (g_busy)
        return PW_ENGINE_BUSY;
    if (pw_engine_has_state())
        pw_engine_discard();
    if (check_environment() < 0 || ensure_staging() < 0)
        return -1;
    memset(g_hdr, 0, sizeof(*g_hdr)); /* not a state until adopted */
    p->header = g_hdr;
    p->header_len = sizeof(*g_hdr);
    p->image = g_kernel_image;
    p->image_len = IMAGE_SPACE;
    p->arena = g_arena.base[0];
    p->arena_len = g_arena.size[0];
    return 0;
}

int pw_engine_adopt(void)
{
    StateHeader *h = g_hdr;
    pw_engine_env now;

    if (h == NULL)
        return -1;
    pw_engine_env_now(&now);
    if (h->magic != STATE_MAGIC || !h->valid || h->env.layout != STATE_MAGIC ||
        h->image_len != IMAGE_SPACE || h->arena_used > g_arena.size[0]) {
        h->magic = 0;
        pw_log("engine: adopt: not a state of this layout");
        return PW_ENGINE_E_LAYOUT;
    }
    if (memcmp(&h->env, &now, sizeof(now)) != 0) {
        pw_engine_env_log("engine: adopt: saved in another setup", &h->env, &now);
        h->magic = 0;
        return PW_ENGINE_E_ENV;
    }
    if (pw_sum32(g_arena.base[0], h->arena_used) != h->arena_sum ||
        pw_sum32(g_kernel_image, h->image_len) != h->image_sum) {
        pw_log("engine: adopt: the state's data is damaged (checksums)");
        h->magic = 0;
        return PW_ENGINE_E_DAMAGED;
    }
    /* What describes this boot comes from this boot, never from the file. */
    h->real_ri_valid = (unsigned int)g_real_ri_valid;
    for (int i = 0; i < RI_WORDS; i++)
        h->real_ri[i] = g_real_ri_valid ? g_real_ri[i] : 0;
    h->loads = 0;
    h->guard = 0;
    h->crumb_count = 0;
    for (int k = 0; k < TRACE_KINDS; k++)
        h->trace_count[k] = 0;
    pw_log("engine: adopted a state from the Memory Stick (%u KB packed)", h->arena_used / 1024);
    return 0;
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
    unsigned int seeds[SEED_WORDS];
    int ret;

    for (int i = 0; i < SEED_WORDS; i++)
        seeds[i] = REG_RESET_SEEDS[i];
    log_seeds("boot", seeds_present(seeds) ? "present" : "MISSING", seeds);

    post_mortem();
    ret = sceKernelRegisterSysEventHandler(&g_handler);

    g_registered = ret >= 0;
    return ret;
}

void pw_engine_shutdown(void)
{
    /* No I/O here (module_stop). If an operation was cut short, release_staging keeps the P9
       record for the next session's post-mortem. */
    if (g_registered)
        sceKernelUnregisterSysEventHandler(&g_handler);
    g_registered = 0;
    release_staging();
}
