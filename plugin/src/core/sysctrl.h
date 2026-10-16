// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_SYSCTRL_H
#define PW_SYSCTRL_H

/*
 * ARK-4 SystemControl kernel API (library SystemCtrlForKernel). Declared here instead of
 * pulling ARK's headers in; stubs live in core/stubs/SystemCtrlForKernel.S.
 */

/*
 * Address of an exported function. `module` is a module name (e.g. "sceChnnlsv"), `library`
 * the export library (NULL: any). Uses ARK's NID resolver, so pre-6.x NIDs work too.
 * Returns 0 if not found.
 */
unsigned int sctrlHENFindFunction(const char *module, const char *library, unsigned int nid);

/* What ARK-4 itself does to quit a game (core/systemctrl/src/exitgame.c): forget the ISO, boot the
   next title in UMD mode, reboot into the XMB. */
int sctrlSESetUmdFile(const char *file);
int sctrlSESetBootConfFileIndex(int index); /* 0 = MODE_UMD */
int sctrlKernelExitVSH(void *param);        /* struct SceKernelLoadExecVSHParam *, NULL here */

#endif
