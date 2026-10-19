// SPDX-License-Identifier: GPL-3.0-or-later
#include "usb.h"

#define BIT_CABLE       0x020 /* pspusb.h PSP_USB_CABLE_CONNECTED */
#define BIT_ESTABLISHED 0x002 /* PSP_USB_CONNECTION_ESTABLISHED */

int hm_usb_step(hm_usb_link *l, unsigned int bits, unsigned int now_ms, int *state)
{
    (void)now_ms;
    if (!l->active) { /* at once: with the cable in, the computer sees the PSP straight away */
        *state = bits & BIT_CABLE ? HM_USB_CONNECTING : HM_USB_NO_CABLE;
        return HM_USB_ACTIVATE;
    }
    if (bits & BIT_ESTABLISHED)
        *state = HM_USB_CONNECTED;
    else if (!(bits & BIT_CABLE)) /* stays active: plugging the cable in connects by itself */
        *state = HM_USB_NO_CABLE;
    else
        *state = HM_USB_CONNECTING;
    if (*state == HM_USB_CONNECTING && l->state != HM_USB_CONNECTING)
        l->t_active = now_ms; /* the wait for a computer counts from the cable */
    l->state = *state;
    return HM_USB_KEEP;
}

int hm_usb_stuck(const hm_usb_link *l, int state, unsigned int now_ms)
{
    return l->active && state == HM_USB_CONNECTING && now_ms - l->t_active >= HM_USB_SETTLE_MS;
}
