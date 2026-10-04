// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_LOG_H
#define PW_LOG_H

/*
 * Plain-text log written next to the PRX (e.g. ms0:/seplugins/pocketwatch/pocketwatch.log).
 * Lines go into a RAM ring buffer first, so logging is safe where file I/O is not
 * (sysevent handlers, suspend/resume). The file is truncated on every boot.
 */

/* module_path: full path of the running PRX, as passed to module_start. */
void pw_log_init(const char *module_path);

/* Append one line (printf-style via stb_sprintf, no floating point). Flushes unless deferred. */
void pw_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* While deferred, lines stay in RAM until pw_log_flush; use around code that must not do I/O. */
void pw_log_set_deferred(int deferred);

/* Write buffered lines to the log file. Must be called from a thread where file I/O is allowed. */
void pw_log_flush(void);

#endif
