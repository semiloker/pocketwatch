// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_NET_H
#define HM_NET_H

#include "dialogs.h"

/*
 * The network in Home (HOM-REQ-017): the system's Internet Browser (sceUtilityHtmlViewer, the
 * firmware's NetFront - the XMB's Network → Internet Browser), connecting by itself through the
 * connections saved in the XMB's Network Settings. The network libraries are loaded for the
 * browser and let go after it, so Home keeps nothing of them while browsing games.
 */

/* Run the browser on url until the user leaves it; frame draws behind it. Writes what happened
   (each step's result, the memory it got) into log (cap bytes). Returns 0 when it ran, < 0 when
   it could not start (log says why). */
int hm_net_browser(const char *url, int cross_enter, int language, hm_frame_fn frame, void *ctx,
                   char *log, int cap);

/* The WLAN switch: 1 on, 0 off, < 0 not known. */
int hm_net_switch(void);

/* The internet's time (HOM-REQ-032): connect through the first connection of the XMB's Network
   Settings, ask an SNTP server (pool.ntp.org, then time.google.com), let it all go again. Blocks
   for up to ~30 s: for a thread of its own. 0 and the time (Unix microseconds), or HM_NET_*;
   log (cap bytes) says each step's result. */
enum {
    HM_NET_SWITCH_OFF = -2,
    HM_NET_NO_PROFILE = -3,
    HM_NET_NO_CONNECTION = -4,
    HM_NET_FAILED = -5
};
int hm_net_time(unsigned long long *unix_us, char *log, int cap);

#endif
