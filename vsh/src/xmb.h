// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_VSH_XMB_H
#define PW_VSH_XMB_H

/* Called by the start-module handler around the next handler (ARK-4's xmbctrl): find the call
   sites before it runs, wrap them after (src/xmb.c). */
void pw_xmb_vsh_before(unsigned int text, unsigned int size);
void pw_xmb_vsh_after(void);
void pw_xmb_sysconf_before(unsigned int text, unsigned int size);
void pw_xmb_sysconf_after(void);

#endif
