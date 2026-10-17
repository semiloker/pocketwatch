// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_VSH_XLOG_H
#define PW_VSH_XLOG_H

/*
 * The XMB plugin's log: lines collect in RAM (the patches run in the module manager's thread,
 * where we do no file I/O) and go to seplugins/pocketwatch/xmb.log from the XMB's own threads.
 * The first write of a boot starts the file again.
 */
void pwx_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void pwx_log_flush(void);

#endif
