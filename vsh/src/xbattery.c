// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspkernel.h>
#include <psppower.h>

#include "xbattery.h"
#include "xlog.h"

#define PERIOD_US (15 * 1000 * 1000)

/* ARK-4: SystemCtrlForUser, KUBridge (src/stubs.S). */
unsigned int sctrlHENFindFunction(const char *module, const char *library, unsigned int nid);
struct pwx_kcall {
    unsigned int arg[12], ret1, ret2;
};
int kuKernelCall(void *func, struct pwx_kcall *args);

/* sceSyscon_driver getters (uOFW include/syscon.h: int f(int *out)); pre-6.x NIDs, which
   sctrlHENFindFunction translates with ARK-4's table. */
#define NID_GET_ELEC  0x373EC933
#define NID_GET_CYCLE 0xB5105D51

static pw_batt_history g_history;
static void *g_get_elec, *g_get_cycle;
static int g_logged;

static int kernel_value(void *f)
{
    volatile int out = PW_BATT_UNKNOWN; /* the kernel function writes here (user memory) */
    struct pwx_kcall a = {{0}, 0, 0};

    if (!f)
        return PW_BATT_UNKNOWN;
    a.arg[0] = (unsigned int)&out;
    return kuKernelCall(f, &a) < 0 || (int)a.ret1 < 0 ? PW_BATT_UNKNOWN : out;
}

static int value(int v)
{
    return v < 0 ? PW_BATT_UNKNOWN : v;
}

void pwx_battery_read(pw_batt_sample *s)
{
    s->t_ms = sceKernelGetSystemTimeLow() / 1000;
    s->present = value(scePowerIsBatteryExist());
    s->charging = value(scePowerIsBatteryCharging());
    s->online = value(scePowerIsPowerOnline());
    s->percent = value(scePowerGetBatteryLifePercent());
    s->fw_minutes = value(scePowerGetBatteryLifeTime());
    s->mv = s->ma = s->temp_c = s->rcap_mah = s->full_mah = s->cycles = PW_BATT_UNKNOWN;
    if (s->present == 1) {
        s->mv = value(scePowerGetBatteryVolt());
        s->temp_c = value(scePowerGetBatteryTemp());
        s->rcap_mah = value(scePowerGetBatteryRemainCapacity());
        s->full_mah = value(scePowerGetBatteryFullCapacity());
        s->ma = kernel_value(g_get_elec);
        s->cycles = kernel_value(g_get_cycle);
    }
    if (!g_logged) {
        g_logged = 1;
        pwx_log("xmb: battery %d%% present %d charging %d online %d system %d min; %d/%d mAh %d mV "
                "%d C; syscon %d mA %d cycles",
                s->percent, s->present, s->charging, s->online, s->fw_minutes, s->rcap_mah,
                s->full_mah, s->mv, s->temp_c, s->ma, s->cycles);
    }
    pw_batt_validate(s);
}

static int sampler(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    for (;;) {
        pw_batt_sample s;

        pwx_battery_read(&s);
        pw_batt_history_add(&g_history, &s);
        sceKernelDelayThread(PERIOD_US);
    }
    return 0;
}

void pwx_battery_start(void)
{
    SceUID th;

    pw_batt_history_reset(&g_history);
    g_get_elec = (void *)sctrlHENFindFunction("sceSYSCON_Driver", "sceSyscon_driver", NID_GET_ELEC);
    g_get_cycle =
        (void *)sctrlHENFindFunction("sceSYSCON_Driver", "sceSyscon_driver", NID_GET_CYCLE);
    th = sceKernelCreateThread("PocketwatchBattery", sampler, 0x30, 0x1000, 0, 0);
    if (th >= 0)
        sceKernelStartThread(th, 0, 0);
}

const pw_batt_history *pwx_battery_history(void)
{
    return &g_history;
}
