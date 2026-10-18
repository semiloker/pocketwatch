// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PWX_XBOOT_H
#define PWX_XBOOT_H

/*
 * Start Pocketwatch Home instead of the XMB (ADR-026, VSH-REQ-009): called first in module_start,
 * which ARK-4 runs while it loads the VSH plugins - before vsh_module starts. When config.txt says
 * start = home and nothing speaks against it (core/bootpick.h), Home's file name goes into ARK-4's
 * config and ARK-4 starts it in place of the XMB. The decision is written to boot.txt.
 * Returns 1 when Home will start (the XMB will not).
 */
int pwx_boot(void);

#endif
