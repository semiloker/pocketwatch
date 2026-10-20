// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_DIALOGS_H
#define HM_DIALOGS_H

/*
 * The firmware's own dialogs inside Home (docs/research/m7-xmb-inventory.md): the Saved Data
 * list of one game (the XMB's own screen, drawn by the firmware over Home's frame; the nickname
 * has Home's own keyboard since ADR-039). While one is open, frame() draws Home's last screen each
 * vblank; the dialog is drawn over it after the GE has finished.
 */

typedef void (*hm_frame_fn)(void *ctx); /* hm_gu_begin .. hm_gu_finish, no hm_gu_end */
typedef int (*hm_status_fn)(void);      /* sceUtility...GetStatus */
typedef void (*hm_update_fn)(void);     /* ...Update(1), ...ShutdownStart() */

/* A dialog's common part (pspUtilityDialogCommon, size bytes of its parameters cleared). */
void hm_dialog_base(void *common, unsigned int size, int cross_enter, int language);
/* Frames until the dialog has gone: Home's frame, the GE done, the dialog's update, the swap. */
void hm_dialog_run(hm_status_fn status, hm_update_fn update, hm_update_fn shutdown,
                   hm_frame_fn frame, void *ctx);

/* The game's saved data (folders that start with its disc ID), with the firmware's details and
   delete. Returns 0 when the dialog ran. */
int hm_dialog_savedata(const char *disc_id, int cross_enter, int language, hm_frame_fn frame,
                       void *ctx);

#endif
