// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_VSH_XBATTERY_H
#define PW_VSH_XBATTERY_H

#include "core/battmodel.h"

/*
 * The battery in the XMB (BAT-REQ-001..008): scePower (user mode) for the system's figures and
 * capacities, the battery's gauge through syscon for the current and the cycle count (kernel
 * functions, called through ARK-4's kuKernelCall). A thread takes a reading every 15 s from the
 * XMB's start, so the Battery page has history to estimate from.
 */

void pwx_battery_start(void);
void pwx_battery_read(pw_batt_sample *s);
const pw_batt_history *pwx_battery_history(void);

#endif
