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

#endif
