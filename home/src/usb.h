// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_USB_H
#define HM_USB_H

/*
 * USB mass storage from Pocketwatch Home (HOM-REQ-011): no detour through the XMB. The system's
 * drivers are loaded and started once and the connection activated once, as ARK-4's menu does
 * (extras/menus/arkMenu/src/usb.cpp). Home never deactivates it by itself: a computer slow to
 * take the card would lose it again (the automatic retry every 3 s of 2026-10-19 did that). Enter
 * on the USB card connects again by hand. While it is on, the card belongs to the computer: Home
 * does no file I/O, and when it ends Home starts again before it writes anything (main.c
 * usb_end, ADR-048): the FAT driver's cached tables are the card as it was before the computer's
 * changes, a write from them put a folder back (2026-10-20/21: files lost, then the PRXs broken),
 * and a user program may not drop them (sceIoDevctl fatms0: 0x0240D81E: 0x800200D1).
 *
 * Another plugin may hold the port (ADR-034): RemoteJoyLite, loaded for games - and so for Home,
 * a homebrew - starts the bus and activates its own device (PID 0x1C9). The bus then answers
 * "already started" (taken as running, and left running at the end), and the storage driver
 * "in progress" until that device is deactivated (home.log 2026-10-19: 0x80243001, 0x80243006).
 * Home deactivates it, retries the storage driver for a while, and activates the other device
 * again when it ends.
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

/* 1 when USB failed because another plugin's device holds the port (the storage driver never
   started: "in progress" for HM_USB_STORAGE_WAIT_MS). */
int hm_usb_busy(void);

/* The firmware's answers (usbacc and the PSP's usb.prx; uOFW src/kd/usbacc/usbacc.c, PSP homebrew
   headers): */
#define HM_USB_ERR_ALREADY     0x80243001u /* the driver is started already */
#define HM_USB_ERR_IN_PROGRESS 0x80243006u /* another device is active on the bus */
#define HM_USB_STORAGE_WAIT_MS 3000        /* how long the storage driver is retried */
#define HM_USB_OTHER_PID       0x1C9       /* RemoteJoyLite's (and usbhostfs') device */

/* 1 when the bus runs after sceUsbStart returned ret (started now, or by another plugin). */
int hm_usb_bus_runs(int ret);
enum { HM_USB_STORAGE_OK, HM_USB_STORAGE_RETRY, HM_USB_STORAGE_FAIL };
/* What to do after the storage driver's sceUsbStart returned ret, waited_ms after the first try. */
int hm_usb_storage_answer(int ret, unsigned int waited_ms);

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
