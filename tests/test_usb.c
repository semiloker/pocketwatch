// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "pwtest.h"
#include "usb.h"

#define ACT   0x200
#define CABLE 0x020
#define EST   0x002

PW_TEST(usb_activates_once_and_never_drops_the_link_by_itself)
{
    hm_usb_link l;
    int st;

    memset(&l, 0, sizeof(l));
    CHECK_EQ(hm_usb_step(&l, CABLE, 1000, &st), HM_USB_ACTIVATE); /* no waiting for anything */
    CHECK_EQ(st, HM_USB_CONNECTING);
    l.active = 1;
    l.t_active = 1000;
    l.state = st;
    /* a computer slow to take the card: kept as it is, however long (the 3 s retry of
       2026-10-19 took it away again); after the settle time Enter may connect again */
    for (unsigned int t = 1250; t < 60000; t += 250) {
        CHECK_EQ(hm_usb_step(&l, ACT | CABLE, t, &st), HM_USB_KEEP);
        CHECK_EQ(st, HM_USB_CONNECTING);
        CHECK_EQ(hm_usb_stuck(&l, st, t), t - 1000 >= HM_USB_SETTLE_MS);
    }
    CHECK_EQ(hm_usb_step(&l, ACT | CABLE | EST, 60000, &st), HM_USB_KEEP);
    CHECK_EQ(st, HM_USB_CONNECTED);
    CHECK_EQ(hm_usb_stuck(&l, st, 90000), 0);
}

PW_TEST(usb_waits_for_the_cable_while_active)
{
    hm_usb_link l;
    int st;

    memset(&l, 0, sizeof(l));
    CHECK_EQ(hm_usb_step(&l, 0, 0, &st), HM_USB_ACTIVATE);
    CHECK_EQ(st, HM_USB_NO_CABLE);
    l.active = 1;
    for (unsigned int t = 0; t < 20000; t += 500) { /* no cable for 20 s: nothing to retry */
        CHECK_EQ(hm_usb_step(&l, ACT, t, &st), HM_USB_KEEP);
        CHECK_EQ(st, HM_USB_NO_CABLE);
        CHECK_EQ(hm_usb_stuck(&l, st, t), 0);
    }
    /* plugged in: the settle time counts from then */
    CHECK_EQ(hm_usb_step(&l, ACT | CABLE, 20100, &st), HM_USB_KEEP);
    CHECK_EQ(st, HM_USB_CONNECTING);
    CHECK_EQ(hm_usb_stuck(&l, st, 20100 + HM_USB_SETTLE_MS - 1), 0);
    CHECK_EQ(hm_usb_stuck(&l, st, 20100 + HM_USB_SETTLE_MS), 1);
}

/* RemoteJoyLite holds the port (home.log 2026-10-19): the bus "already started" runs; the storage
   driver "in progress" is tried again until its device has let go, then given up. */
PW_TEST(usb_shares_the_port_with_another_plugins_device)
{
    CHECK(hm_usb_bus_runs(0));
    CHECK(hm_usb_bus_runs((int)0x80243001u));
    CHECK(!hm_usb_bus_runs((int)0x80243005u)); /* no such driver */
    CHECK_EQ(hm_usb_storage_answer(0, 0), HM_USB_STORAGE_OK);
    CHECK_EQ(hm_usb_storage_answer((int)0x80243001u, 500), HM_USB_STORAGE_OK);
    CHECK_EQ(hm_usb_storage_answer((int)0x80243006u, 0), HM_USB_STORAGE_RETRY);
    CHECK_EQ(hm_usb_storage_answer((int)0x80243006u, HM_USB_STORAGE_WAIT_MS - 1),
             HM_USB_STORAGE_RETRY);
    CHECK_EQ(hm_usb_storage_answer((int)0x80243006u, HM_USB_STORAGE_WAIT_MS), HM_USB_STORAGE_FAIL);
    CHECK_EQ(hm_usb_storage_answer((int)0x80243007u, 0), HM_USB_STORAGE_FAIL); /* no bus */
}
