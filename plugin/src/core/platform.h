// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_PLATFORM_H
#define PW_PLATFORM_H

/*
 * The few OS services that portable modules need. Implemented by core/platform_psp.c on the
 * PSP and by tests/platform_host.c for host-side unit tests, so logic modules stay testable.
 */

/* Mask interrupts (PSP) and return the previous state for pw_irq_unlock. */
unsigned int pw_irq_lock(void);
void pw_irq_unlock(unsigned int state);

/* Write len bytes to path, appending or truncating first. Returns bytes written or < 0. */
int pw_file_write(const char *path, const void *data, unsigned int len, int append);

/* Size of a file in bytes, or < 0 if it does not exist. */
int pw_file_size(const char *path);

#endif
