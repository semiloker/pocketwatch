// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_DIALOG_H
#define PW_DIALOG_H

#include "ui/ui.h"

/*
 * Dialogs in the system's message-dialog style (docs/ui/README.md, concept dialog-confirm.svg):
 * white rules around centred text over the dimmed game, Yes/No, the hint bar. Each runs its own
 * loop until answered. line2 may be NULL.
 */

/* Returns 1 for Yes, 0 for No / Back / HOME. */
int pw_dialog_confirm(pw_ui *ui, const char *line1, const char *line2, int default_yes);
void pw_dialog_message(pw_ui *ui, const char *line1, const char *line2);

/* One frame with a busy indicator and the text, shown until the next frame is drawn. */
void pw_dialog_busy(pw_ui *ui, const char *text);

#endif
