// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PWX_XBOOT_H
#define PWX_XBOOT_H

/*
 * Start Pocketwatch Home instead of the XMB (ADR-026, ADR-027, VSH-REQ-009).
 *
 * ARK-4 runs the VSH plugins' module_start just before it starts sceMediaSync; sceMediaSync's own
 * start then runs ARK-4's processSettings, which clears ARKConfig.launcher; vsh_module's start
 * comes later and starts the launcher instead of the XMB if the field is set
 * (docs/research/m7-boot-shell.md). So:
 * - pwx_boot_decide, first in module_start: reads config.txt and the markers, decides, writes
 *   boot.txt (file I/O is fine here);
 * - pwx_boot_module, from the start-module handler after the older handlers ran: on sceMediaSync,
 *   after ARK-4's processSettings, it writes the launcher field (no file I/O);
 * - pwx_boot_xmb_starts, as vsh_module starts: if Home was chosen, it did not happen - the RAM log
 *   says why.
 */
void pwx_boot_decide(void);
void pwx_boot_module(const char *modname);
void pwx_boot_xmb_starts(void);

/* From a thread once the XMB runs: a Home that was chosen but did not start is not Home's
   fault - its boot-pending marker goes, so the next start tries again. */
void pwx_boot_tidy(void);

#endif
