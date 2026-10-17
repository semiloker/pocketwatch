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

/* Read up to cap bytes of a file from its start. Returns bytes read or < 0. */
int pw_file_read(const char *path, void *data, unsigned int cap);

/* A file open for streaming: write = create or truncate, else read. Returns a handle or < 0. */
int pw_file_open(const char *path, int write);
int pw_file_put(int fd, const void *data, unsigned int len); /* bytes written or < 0 */
int pw_file_get(int fd, void *data, unsigned int len);       /* bytes read or < 0 */
void pw_file_close(int fd);

int pw_file_remove(const char *path);
int pw_dir_make(const char *path); /* < 0 also when it exists */
int pw_dir_remove(const char *path);

/* Local date and time; the same layout as the PSP's ScePspDateTime. */
typedef struct {
    unsigned short year, month, day, hour, minute, second;
    unsigned int microsecond;
} pw_datetime;

void pw_local_time(pw_datetime *t);

void pw_delay_ms(unsigned int ms);

/*
 * The buttons held now (PSP_CTRL_* bits, HOME and NOTE included). Read from the controller's
 * kernel latch, never through sceCtrlPeek/ReadBuffer*: ARK-4 hijacks those four and starts its
 * exit-to-launcher / exit-to-XMB combos (L+R+START/SELECT+DOWN) from inside whichever thread calls
 * them - ours too, in the middle of a save or load (SHL-REQ-012). The host returns 0.
 */
unsigned int pw_buttons_held(void);
unsigned int pw_time_ms(void); /* a monotonic millisecond counter (wraps) */

#endif
