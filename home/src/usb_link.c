// SPDX-License-Identifier: GPL-3.0-or-later
#include "usb.h"

#define BIT_ACTIVATED   0x200 /* pspusb.h PSP_USB_ACTIVATED */
#define BIT_CABLE       0x020 /* PSP_USB_CABLE_CONNECTED */
#define BIT_ESTABLISHED 0x002 /* PSP_USB_CONNECTION_ESTABLISHED */

int hm_usb_step(hm_usb_link *l, unsigned int bits, unsigned int now_ms, int *state)
{
    if (!l->active) { /* at once: with the cable in, the computer sees the PSP straight away */
        *state = bits & BIT_CABLE ? HM_USB_CONNECTING : HM_USB_NO_CABLE;
        return HM_USB_ACTIVATE;
    }
    if (bits & BIT_ESTABLISHED) {
        *state = HM_USB_CONNECTED;
        l->retries = 0;
        return HM_USB_KEEP;
    }
    if (!(bits & BIT_CABLE)) { /* stays active: plugging the cable in connects by itself */
        *state = HM_USB_NO_CABLE;
        l->t_active = now_ms;
        return HM_USB_KEEP;
    }
    *state = HM_USB_CONNECTING;
    if (now_ms - l->t_active >= HM_USB_SETTLE_MS) {
        /* A cable, but the computer never took the device: what makes the XMB need a second
           press. Activate again, by ourselves. */
        l->retries++;
        return HM_USB_REACTIVATE;
    }
    return HM_USB_KEEP;
}
