// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "pwtest.h"
#include "usb.h"

#define ACT   0x200
#define CABLE 0x020
#define EST   0x002

PW_TEST(usb_activates_at_once_and_retries_a_stuck_link)
{
    hm_usb_link l;
    int st;

    memset(&l, 0, sizeof(l));
    CHECK_EQ(hm_usb_step(&l, CABLE, 1000, &st), HM_USB_ACTIVATE); /* no waiting for anything */
    CHECK_EQ(st, HM_USB_CONNECTING);
    l.active = 1;
    l.t_active = 1000;
    CHECK_EQ(hm_usb_step(&l, ACT | CABLE, 2000, &st), HM_USB_KEEP);
    CHECK_EQ(st, HM_USB_CONNECTING);
    /* the computer never took it: activated again by itself (the XMB's "second press") */
    CHECK_EQ(hm_usb_step(&l, ACT | CABLE, 1000 + HM_USB_SETTLE_MS, &st), HM_USB_REACTIVATE);
    CHECK_EQ(l.retries, 1);
    l.t_active = 4000;
    CHECK_EQ(hm_usb_step(&l, ACT | CABLE | EST, 4500, &st), HM_USB_KEEP);
    CHECK_EQ(st, HM_USB_CONNECTED);
    CHECK_EQ(l.retries, 0);
}

PW_TEST(usb_waits_for_the_cable_while_active)
{
    hm_usb_link l;
    int st;

    memset(&l, 0, sizeof(l));
    CHECK_EQ(hm_usb_step(&l, 0, 0, &st), HM_USB_ACTIVATE);
    CHECK_EQ(st, HM_USB_NO_CABLE);
    l.active = 1;
    for (unsigned int t = 0; t < 20000; t += 500) { /* no cable for 20 s: no retries */
        CHECK_EQ(hm_usb_step(&l, ACT, t, &st), HM_USB_KEEP);
        CHECK_EQ(st, HM_USB_NO_CABLE);
    }
    /* plugged in: the settle time counts from then */
    CHECK_EQ(hm_usb_step(&l, ACT | CABLE, 20100, &st), HM_USB_KEEP);
    CHECK_EQ(st, HM_USB_CONNECTING);
}
