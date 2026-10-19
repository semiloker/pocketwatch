// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_USB_H
#define HM_USB_H

/*
 * USB mass storage from Pocketwatch Home (HOM-REQ-011): no detour through the XMB. The system's
 * drivers are loaded and started once and the connection activated once, exactly as ARK-4's menu
 * does (extras/menus/arkMenu/src/usb.cpp). Home never deactivates it by itself: a computer slow
 * to take the card would lose it again (the automatic retry every 3 s of 2026-10-19 did that; the
 * owner's PSP never connected). Enter on the USB card connects again by hand. While it is on, the
 * card belongs to the computer: Home does no file I/O.
 */

enum { HM_USB_OFF, HM_USB_NO_CABLE, HM_USB_CONNECTING, HM_USB_CONNECTED, HM_USB_FAILED };

/* Loads and starts the drivers, not yet the connection; HM_USB_FAILED when the bus or the
   storage driver does not start. hm_usb_report says what each step returned (for home.log, to
   be written before the first hm_usb_poll: the card is the computer's from then). */
int hm_usb_start(unsigned int now_ms);
int hm_usb_poll(unsigned int now_ms);  /* the state now; activates the connection the first time */
int hm_usb_again(unsigned int now_ms); /* deactivate, activate: the owner asked (Enter) */
int hm_usb_may_retry(int state, unsigned int now_ms); /* hm_usb_stuck for the link now */
void hm_usb_stop(void);
int hm_usb_report(char *out, int cap); /* every step and the states seen since the start */

/* The decision of hm_usb_poll, portable (tests/test_usb.c). */
typedef struct {
    int active;            /* sceUsbActivate is in effect */
    unsigned int t_active; /* since when: activated, or the cable in without a computer */
    int state;             /* HM_USB_* of the last step */
    int retries;           /* by hand */
} hm_usb_link;

enum { HM_USB_KEEP, HM_USB_ACTIVATE };

#define HM_USB_SETTLE_MS 3000 /* a cable but no computer after this: Enter connects again */

/* What to do for the state bits of sceUsbGetState (PSP_USB_*); *state gets HM_USB_*. */
int hm_usb_step(hm_usb_link *l, unsigned int bits, unsigned int now_ms, int *state);
/* 1 when connecting again by hand makes sense: a cable, no computer, for HM_USB_SETTLE_MS. */
int hm_usb_stuck(const hm_usb_link *l, int state, unsigned int now_ms);

#endif
