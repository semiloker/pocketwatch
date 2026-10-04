#ifndef PW_LOG_H
#define PW_LOG_H

/*
 * Plain-text log written next to the PRX (e.g. ms0:/seplugins/pocketwatch/pocketwatch.log).
 * The file is truncated on every boot and each line is flushed immediately, so the
 * log survives a crash or a hard power-off.
 */

/* module_path: full path of the running PRX, as passed to module_start. */
void pw_log_init(const char *module_path);

/* Append one line. Formatting is done by sysclib's prnt (%s %d %u %x %p %c). */
void pw_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
