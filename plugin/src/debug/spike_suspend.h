// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_SPIKE_SUSPEND_H
#define PW_SPIKE_SUSPEND_H

/*
 * Spike E1 (debug builds, L + R + SELECT + RIGHT): the first rung of the savestate engine.
 * Requests a real system suspend with an RTC wake-up 100 ms later and, at sysevent PHASE0_0
 * (everything quiesced), copies the 4 MB kernel image into a P9 staging buffer. Nothing is
 * restored: to the game it looks like a quick sleep/wake. Logs the full sysevent trace,
 * timings and a sanity check of the captured image.
 */

/* Register/unregister the sysevent handler (call from the probe thread's start/stop). */
int pw_spike_suspend_init(void);
void pw_spike_suspend_shutdown(void);

void pw_spike_suspend(void);

#endif
