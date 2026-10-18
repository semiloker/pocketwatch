// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspkernel.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <string.h>

#include "usb.h"

#define USB_PID 0x1C8 /* mass storage */

/* ARK-4 KUBridge (src/stubs.S): load a kernel module for this user program. */
SceUID kuKernelLoadModule(const char *path, int flags, SceKernelLMOption *option);

static int g_loaded, g_started;
static hm_usb_link g_link;

/* The system's USB storage drivers, in ARK-4's menu's order (usb.prx is already there). */
static int load_drivers(void)
{
    static const char *const mods[] = {
        "flash0:/kd/semawm.prx",    "flash0:/kd/usbstor.prx",     "flash0:/kd/usbstormgr.prx",
        "flash0:/kd/usbstorms.prx", "flash0:/kd/usbstorboot.prx",
    };

    if (g_loaded)
        return 0;
    for (unsigned int i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
        SceUID id = kuKernelLoadModule(mods[i], 0, NULL);

        if (id >= 0)
            sceKernelStartModule(id, 0, NULL, NULL, NULL);
    }
    g_loaded = 1;
    return 0;
}

int hm_usb_start(unsigned int now_ms)
{
    load_drivers();
    if (!g_started) {
        if (sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, 0) < 0 ||
            sceUsbStart(PSP_USBSTOR_DRIVERNAME, 0, 0) < 0) {
            sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, 0);
            return HM_USB_FAILED;
        }
        g_started = 1;
    }
    memset(&g_link, 0, sizeof(g_link));
    return hm_usb_poll(now_ms);
}

int hm_usb_poll(unsigned int now_ms)
{
    int state = HM_USB_OFF, what;

    if (!g_started)
        return HM_USB_OFF;
    what = hm_usb_step(&g_link, (unsigned int)sceUsbGetState(), now_ms, &state);
    if (what == HM_USB_REACTIVATE) {
        sceUsbDeactivate(USB_PID);
        sceKernelDelayThread(20 * 1000);
    }
    if (what != HM_USB_KEEP) {
        if (sceUsbActivate(USB_PID) < 0)
            return HM_USB_FAILED;
        g_link.active = 1;
        g_link.t_active = now_ms;
    }
    return state;
}

void hm_usb_stop(void)
{
    if (!g_started)
        return;
    if (g_link.active)
        sceUsbDeactivate(USB_PID);
    sceUsbStop(PSP_USBSTOR_DRIVERNAME, 0, 0);
    sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, 0);
    g_link.active = 0;
    g_started = 0;
}
