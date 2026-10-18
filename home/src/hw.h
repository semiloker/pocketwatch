// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_HW_H
#define HM_HW_H

#include <psptypes.h>

/* Pocketwatch Home's view of the PSP: the buttons, the clock, the battery, the card. The screen is
   gfx_gu.h's. */

int hm_hw_init(void);             /* controller, HOME, exit callback, the GE */
int hm_hw_home_ours(void);        /* HOME reaches Home (the system's quit dialog is off) */
unsigned int hm_hw_buttons(void); /* HOME included when hm_hw_home_ours */
unsigned int hm_hw_now_ms(void);
int hm_hw_exit_requested(void); /* HOME -> Quit was chosen */

/* "10/18 14:05" in the system's date and time format; the same for a file's time (with the year).
 */
void hm_hw_clock(char *out, int cap);
void hm_hw_format_time(const ScePspDateTime *t, char *out, int cap);

/* System Information (HOM-REQ-013): "PSP-3000", "6.61", "ARK-4 4.20.69", "00:1F:...". */
void hm_hw_model(char *out, int cap);
void hm_hw_firmware(char *out, int cap);
void hm_hw_cfw(char *out, int cap);
void hm_hw_mac(char *out, int cap);
int hm_hw_battery(int *charging); /* percent or -1 */
int hm_hw_cross_enter(void);      /* the system's Enter button is X */

/* The marker files the XMB plugin reads at the next start (ADR-026): the RTC tick now. */
void hm_hw_touch(const char *path);
void hm_hw_remove(const char *path);
int hm_hw_language(void);         /* PSP_SYSTEMPARAM_LANGUAGE_* */
unsigned int hm_hw_free_mb(void); /* free space on ms0:, MB */

int hm_hw_read_file(const char *path, void *buf, unsigned int cap); /* bytes or < 0 */

#endif
