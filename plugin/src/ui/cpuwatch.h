// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_CPUWATCH_H
#define PW_CPUWATCH_H

/*
 * Who used the CPU while a menu was open: every thread's run time (SceKernelThreadInfo.runClocks)
 * at the start and at the end, the biggest users logged, the idle threads included (their share is
 * the CPU left over). The menu's frame rate depends on it (docs/ui/styles.md).
 */
void pw_cpuwatch_start(void);
void pw_cpuwatch_stop(const char *what);

#endif
