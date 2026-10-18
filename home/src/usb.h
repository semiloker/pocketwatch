// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_USB_H
#define HM_USB_H

/*
 * USB mass storage from Pocketwatch Home (HOM-REQ-011): no detour through the XMB. The system's
 * drivers are loaded once, as ARK-4's menu does (extras/menus/arkMenu/src/usb.cpp); then the
 * connection is watched and retried until the computer has the Memory Stick, so one press is
 * enough. While it is on, the card belongs to the computer: Home does no file I/O.
 */

enum { HM_USB_OFF, HM_USB_NO_CABLE, HM_USB_CONNECTING, HM_USB_CONNECTED, HM_USB_FAILED };

int hm_usb_start(unsigned int now_ms); /* HM_USB_FAILED when the drivers do not start */
int hm_usb_poll(unsigned int now_ms);  /* the state now; retries a stuck connection */
void hm_usb_stop(void);

/* The decision of hm_usb_poll, portable (tests/test_usb.c). */
typedef struct {
    int active;            /* sceUsbActivate is in effect */
    unsigned int t_active; /* since when */
    int retries;
} hm_usb_link;

enum { HM_USB_KEEP, HM_USB_ACTIVATE, HM_USB_REACTIVATE };

#define HM_USB_SETTLE_MS 3000 /* a cable but no computer after this: activate again */

/* What to do for the state bits of sceUsbGetState (PSP_USB_*); *state gets HM_USB_*. */
int hm_usb_step(hm_usb_link *l, unsigned int bits, unsigned int now_ms, int *state);

#endif
