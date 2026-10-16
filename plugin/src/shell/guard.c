// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspkernel.h>
#include <psploadcore.h>
#include <pspsysmem_kernel.h>
#include <string.h>

#include "core/log.h"
#include "shell/guard.h"
#include "ui/i18n.h"

/* core/stubs/scePower_driver.S, core/stubs/sceSyscon_driver.S */
int scePowerGetBatteryLifePercent(void);
int scePowerIsPowerOnline(void);
int scePowerIsBatteryExist(void);
int sceSysconGetWlanPowerCtrl(void);

/* Module names read from the PRX files (2026-10-16): Custom Home's cushome.prx and its
   pspstates_cushome.prx; ARK-4's VSH menu tests the same USB module (vshctrl/vshmenu.c). */
static const char *const g_conflicts[] = {"CustomHOME", "PspStates"};
#define USB_STORAGE_MODULE "sceUSB_Stor_Driver"
#define USER_SIZE          0x01800000u /* the engine's only supported game RAM size */

static int g_last = -1;

static int check(void)
{
    PspSysmemPartitionInfo p2;

    memset(&p2, 0, sizeof(p2));
    p2.size = sizeof(p2);
    if (sceKernelQueryMemoryPartitionInfo(2, &p2) >= 0 && p2.memsize != USER_SIZE)
        return PW_GUARD_HIGHMEM;
    for (unsigned int i = 0; i < sizeof(g_conflicts) / sizeof(g_conflicts[0]); i++)
        if (sceKernelFindModuleByName(g_conflicts[i]) != NULL)
            return PW_GUARD_PLUGIN;
    if (sceKernelFindModuleByName(USB_STORAGE_MODULE) != NULL)
        return PW_GUARD_USB;
    if (sceSysconGetWlanPowerCtrl() > 0)
        return PW_GUARD_WLAN;
    if (scePowerIsBatteryExist() > 0 && scePowerIsPowerOnline() <= 0) {
        int pct = scePowerGetBatteryLifePercent();

        if (pct >= 0 && pct < PW_GUARD_BATTERY_MIN)
            return PW_GUARD_BATTERY;
    }
    return PW_GUARD_OK;
}

int pw_guard_states(void)
{
    int g = check();

    if (g != g_last) { /* log changes only: the hub asks every second */
        if (g != PW_GUARD_OK || g_last > 0)
            pw_log("guard: states %s%s", g == PW_GUARD_OK ? "allowed again" : "refused: ",
                   g == PW_GUARD_OK ? "" : pw_guard_reason(g));
        g_last = g;
    }
    return g;
}

const char *pw_guard_reason(int guard)
{
    switch (guard) {
    case PW_GUARD_BATTERY:
        return pw_tr(PW_S_G_BATTERY);
    case PW_GUARD_USB:
        return pw_tr(PW_S_G_USB);
    case PW_GUARD_WLAN:
        return pw_tr(PW_S_G_WLAN);
    case PW_GUARD_PLUGIN:
        return pw_tr(PW_S_G_PLUGIN);
    case PW_GUARD_HIGHMEM:
        return pw_tr(PW_S_G_HIGHMEM);
    default:
        return "";
    }
}

const char *pw_guard_hint(int guard)
{
    switch (guard) {
    case PW_GUARD_BATTERY:
        return pw_tr(PW_S_H_BATTERY);
    case PW_GUARD_USB:
        return pw_tr(PW_S_H_USB);
    case PW_GUARD_WLAN:
        return pw_tr(PW_S_H_WLAN);
    case PW_GUARD_PLUGIN:
        return pw_tr(PW_S_H_PLUGIN);
    case PW_GUARD_HIGHMEM:
        return pw_tr(PW_S_H_HIGHMEM);
    default:
        return 0;
    }
}
