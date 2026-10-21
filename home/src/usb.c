// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspkernel.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <stdio.h>
#include <string.h>

#include "usb.h"

#define USB_PID     0x1C8 /* mass storage */
#define EF_DRIVER   "USBStorEFlash_Driver"
#define STATES_KEPT 8

/* ARK-4 KUBridge (src/stubs.S): load a kernel module for this user program. */
SceUID kuKernelLoadModule(const char *path, int flags, SceKernelLMOption *option);

/* ARK-4's menu's list and order (usb.prx itself is already there; usbstoreflash.prx is the PSP
   Go's internal storage and fails elsewhere, harmless). */
static const char *const k_mods[] = {
    "semawm", "usbstor", "usbstormgr", "usbstorms", "usbstoreflash", "usbstorboot",
};
#define MODS ((int)(sizeof(k_mods) / sizeof(k_mods[0])))

static int g_loaded, g_started;
static int g_load_ret[MODS], g_bus_ret, g_stor_ret, g_act_ret;
static int g_bus_ours;     /* the bus was started here (else another plugin's: left running) */
static int g_stor_pending; /* the storage driver not started yet: retried by hm_usb_poll */
static int g_stor_tries;
static unsigned int g_other_bits; /* the bus's state before: another device active? */
static int g_other_ret = 1;       /* its deactivation (1: none was active) */
static unsigned int g_t_start;
static struct {
    unsigned int bits, t;
} g_seen[STATES_KEPT];
static int g_nseen;
static hm_usb_link g_link;

static void load_drivers(void)
{
    char path[48];

    if (g_loaded)
        return;
    for (int i = 0; i < MODS; i++) {
        SceUID id;

        snprintf(path, sizeof(path), "flash0:/kd/%s.prx", k_mods[i]);
        id = kuKernelLoadModule(path, 0, NULL);
        g_load_ret[i] = id < 0 ? id : sceKernelStartModule(id, 0, NULL, NULL, NULL);
    }
    g_loaded = 1;
}

/* Everything started here stopped again; another plugin's device back as it was. */
static void stop_all(void)
{
    if (g_link.active)
        sceUsbDeactivate(USB_PID);
    g_link.active = 0;
    if (!g_stor_pending) {
        sceUsbStop(EF_DRIVER, 0, 0);
        sceUsbStop(PSP_USBSTOR_DRIVERNAME, 0, 0); /* (RemoteJoyLite's hook reactivates it) */
    }
    if (g_bus_ours)
        sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, 0);
    if (g_other_ret == 0) /* and if its own hook did not: an error, harmless */
        sceUsbActivate(HM_USB_OTHER_PID);
    g_stor_pending = 0;
    g_started = 0;
}

/* The storage driver, again while another device's link winds down. */
static int try_storage(unsigned int now_ms)
{
    int r;

    g_stor_tries++;
    g_stor_ret = sceUsbStart(PSP_USBSTOR_DRIVERNAME, 0, 0);
    r = hm_usb_storage_answer(g_stor_ret, now_ms - g_t_start);
    if (r == HM_USB_STORAGE_RETRY)
        return HM_USB_CONNECTING;
    g_stor_pending = 0;
    if (r == HM_USB_STORAGE_FAIL) {
        g_stor_pending = 1; /* (nothing of it to stop) */
        stop_all();
        return HM_USB_FAILED;
    }
    sceUsbStart(EF_DRIVER, 0, 0); /* as ARK-4's menu; not there on a PSP-3000 */
    return HM_USB_CONNECTING;
}

int hm_usb_start(unsigned int now_ms)
{
    load_drivers();
    memset(&g_link, 0, sizeof(g_link));
    g_nseen = 0;
    g_act_ret = 1; /* not yet */
    g_t_start = now_ms;
    if (g_started)
        return HM_USB_CONNECTING;
    g_bus_ret = sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, 0);
    if (!hm_usb_bus_runs(g_bus_ret))
        return HM_USB_FAILED;
    g_bus_ours = g_bus_ret == 0;
    g_other_ret = 1;
    g_other_bits = (unsigned int)sceUsbGetState();
    if (g_other_bits & PSP_USB_ACTIVATED) /* another plugin's device holds the port */
        g_other_ret = sceUsbDeactivate(HM_USB_OTHER_PID);
    g_started = 1;
    g_stor_pending = 1;
    g_stor_tries = 0;
    return try_storage(now_ms);
}

int hm_usb_busy(void)
{
    return (unsigned int)g_stor_ret == HM_USB_ERR_IN_PROGRESS;
}

static void seen(unsigned int bits, unsigned int now_ms)
{
    if (g_nseen > 0 && g_seen[g_nseen - 1].bits == bits)
        return;
    if (g_nseen == STATES_KEPT) /* keep the first ones and the last */
        g_nseen--;
    g_seen[g_nseen].bits = bits;
    g_seen[g_nseen].t = now_ms - g_t_start;
    g_nseen++;
}

int hm_usb_poll(unsigned int now_ms)
{
    int state = HM_USB_OFF;
    unsigned int bits;

    if (!g_started)
        return HM_USB_OFF;
    if (g_stor_pending) {
        int s = try_storage(now_ms);

        if (g_stor_pending || s == HM_USB_FAILED)
            return s;
    }
    bits = (unsigned int)sceUsbGetState();
    seen(bits, now_ms);
    if (hm_usb_step(&g_link, bits, now_ms, &state) == HM_USB_ACTIVATE) {
        g_act_ret = sceUsbActivate(USB_PID);
        if (g_act_ret < 0)
            return HM_USB_FAILED;
        g_link.active = 1;
        g_link.t_active = now_ms;
        g_link.state = state;
    }
    return state;
}

int hm_usb_may_retry(int state, unsigned int now_ms)
{
    return hm_usb_stuck(&g_link, state, now_ms);
}

int hm_usb_again(unsigned int now_ms)
{
    if (!g_started || g_stor_pending)
        return hm_usb_poll(now_ms);
    if (g_link.active)
        sceUsbDeactivate(USB_PID);
    sceKernelDelayThread(100 * 1000); /* the computer sees the PSP go */
    g_link.active = 0;
    g_link.retries++;
    return hm_usb_poll(now_ms);
}

void hm_usb_stop(void)
{
    if (g_started)
        stop_all();
}

int hm_usb_report(char *out, int cap)
{
    int n = snprintf(out, (size_t)cap, "home usb: modules");

    for (int i = 0; i < MODS && n < cap; i++)
        n += snprintf(out + n, (size_t)(cap - n), " %s %X", k_mods[i], g_load_ret[i]);
    if (n < cap)
        n += snprintf(out + n, (size_t)(cap - n),
                      "; start bus %X (%s), before %X (other device off %X), storage %X after %d "
                      "tries; activate %X;",
                      g_bus_ret, g_bus_ours ? "ours" : "another's", g_other_bits, g_other_ret,
                      g_stor_ret, g_stor_tries, g_act_ret);
    for (int i = 0; i < g_nseen && n < cap; i++)
        n += snprintf(out + n, (size_t)(cap - n), " %X at %u ms", g_seen[i].bits, g_seen[i].t);
    if (n < cap)
        n += snprintf(out + n, (size_t)(cap - n), "; %d again by hand", g_link.retries);
    return n < cap ? n : cap - 1;
}
