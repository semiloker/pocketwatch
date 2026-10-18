// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_HW_H
#define HM_HW_H

#include "ui/draw.h"

/* Pocketwatch Home's view of the PSP: two frames in RAM shown in turn, the buttons, the clock,
   the battery, the card. */

int hm_hw_init(void);         /* display, controller, exit callback */
pw_canvas *hm_hw_begin(void); /* the frame to draw next (480 x 272, stride 512) */
void hm_hw_present(void);     /* show it at the next vblank, wait for it */
unsigned int hm_hw_buttons(void);
unsigned int hm_hw_now_ms(void);
int hm_hw_exit_requested(void); /* HOME -> Quit was chosen */

/* "10/18 14:05" in the system's date and time format. */
void hm_hw_clock(char *out, int cap);
int hm_hw_battery(int *charging); /* percent or -1 */
int hm_hw_cross_enter(void);      /* the system's Enter button is X */

/* The marker files the XMB plugin reads at the next start (ADR-026). */
void hm_hw_touch(const char *path);
void hm_hw_remove(const char *path);
int hm_hw_language(void);         /* PSP_SYSTEMPARAM_LANGUAGE_* */
unsigned int hm_hw_free_mb(void); /* free space on ms0:, MB */

int hm_hw_read_file(const char *path, void *buf, unsigned int cap); /* bytes or < 0 */

#endif
