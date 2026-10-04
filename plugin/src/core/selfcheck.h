// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_SELFCHECK_H
#define PW_SELFCHECK_H

/*
 * Boot-time sanity checks that only the real device can answer.
 *
 * pw_selfcheck_imports walks this module's import stubs. On 6.xx the CFW translates old NIDs
 * to the randomized ones; when it cannot, ARK-4 writes `syscall 0x15` into the stub and a call
 * silently fails. Every unresolved import is logged. Returns the number of unresolved imports.
 */
int pw_selfcheck_imports(void);

#endif
