# Battery — specification

| | |
|---|---|
| Area code | `BAT` |
| Status | Implemented in the XMB 2026-10-17 (Settings → Battery, ADR-024; branch `feature/psp-xmb-overhaul`), host-tested; first in-game reading on the device 2026-10-17; Home's Battery page with the current, our estimate and Pocketwatch's calibration run 2026-10-20 (ADR-042); device tests B1, HM30 pending |
| Code | [`core/battmodel.c`](../../plugin/src/core/battmodel.c) (portable model, estimates), [`vsh/src/xbattery.c`](../../vsh/src/xbattery.c) (readings, the 15 s thread), [`vsh/src/xsettings.c`](../../vsh/src/xsettings.c) (the page's rows); Home: [`home/src/hw.c`](../../home/src/hw.c) (its battery thread, the calibration run), [`home/src/battcal.c`](../../home/src/battcal.c) (the run's file and result, portable), [`home/src/pages.c`](../../home/src/pages.c) (`battery_page`); research [m7-battery-calibration](../research/m7-battery-calibration.md) |

## A. Overview

- **Purpose:** show the battery as it is - what the system reports, what the battery's own gauge
  measures, and how long it will last - without inventing precision.
- **Current state:** XMB → Settings → Battery: Charge, Status, Time left / Time to full,
  Voltage, Current, Temperature, Capacity (remaining of full, mAh), Charge cycles - the XMB's own
  list. (2026-10-17 it was briefly a page of the game's hub; moved by ADR-024.)
- **Target state:** battery history across sessions (PROPOSED); live refresh while the page is
  open (the XMB's list shows the values of the moment it opened).

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `BAT-REQ-001` | Readings come from the system only: scePower (percentage, charging, external power, the firmware's estimate, voltage, temperature, remaining and full capacity) and the battery's gauge through syscon (current, cycles). | P1 | Implemented. VERIFIED on the device (game log 2026-10-17: 4098 mV, -224 mA discharging, 20 C, full 1790 mAh, 0 cycles); syscon's RCap read 74 at 83 % - not mAh (INFERRED: 20 mAh steps), so not used | code review, device test B1 |
| `BAT-REQ-002` | A value the system does not give, or gives outside the range a PSP battery can have, is shown as "Not available", never as 0; without a battery nothing is shown as measured. | P1 | Implemented | `battery_validation_turns_misreads_into_unknown` |
| `BAT-REQ-003` | The time left is "Estimating..." until there is history; then the firmware's figure marked "(system)"; after a minute of readings ours, "about": remaining capacity / average current of the run, or the slope of the percentage over ten minutes. Never shown as exact. | P1 | Implemented | `battery_estimate_needs_history_then_uses_the_current`, `battery_slope_when_there_is_no_current` |
| `BAT-REQ-004` | A change of charging or external power, a gap of more than five minutes, or time running backwards (a state was loaded) starts a new run of history; old readings never mix into a new estimate. | P1 | Implemented | `battery_runs_restart_on_power_changes_gaps_and_loads` |
| `BAT-REQ-005` | While charging, the time to full is the missing capacity at the average charge current, "about" (optimistic near the end, the charger slows down). | P2 | Implemented | `battery_charging_time_to_full` |
| `BAT-REQ-006` | Readings cost nothing visible: in the XMB one every 15 s from a low-priority thread (scePower calls and two syscon getters through kuKernelCall), one more when the page opens; nothing in a game (except the performance overlay's, when the owner turns it on: PRF-REQ-003). | P1 | Implemented (ADR-024) | code review, device test B1 |
| `BAT-REQ-007` | Health (capacity relative to the design capacity) is shown only when the design capacity is known. | P2 | Planned: the design capacity is not known (RESEARCH REQUIRED: syscon `GetIFC`/`GetInfo` contents) | - |
| `BAT-REQ-008` | Home's Battery page (System → Battery) shows the charge and state (on the battery / charging / on external power), our time left or to full ("about", BAT-REQ-003; the system's figure before there is history), the gauge's current (read every 10 s by Home's battery thread, never in the frame loop), voltage, temperature, capacity left and full, cycles. | P1 | Implemented 2026-10-20 (ADR-042); PPSSPP checked; HM30 pending | PPSSPP, HM30 |
| `BAT-REQ-009` | A calibration run: Battery → Calibration → Start; "Charge it full" until the charger stops (98 % or more, not charging), "Full: unplug the charger", then "Measuring: leave Home on" until the PSP turns itself off; a sample (time, %, mV, mA, mAh left, external power) every 30 s in `battery-cal.txt`; no automatic sleep meanwhile; the charger again or Stop ends it; a run Home was left in goes on at Home's next start while the PSP is still on its battery. | P1 | Implemented 2026-10-20 (ADR-042) | HM30 |
| `BAT-REQ-010` | The run's result: the capacity delivered (the current integrated; a gap of over 120 s and the charge lost in it left out; the gauge's drop where no current was read), scaled to 100 % of the charge it covered when that is 30 % or more, kept in `battery.txt` and shown as "Measured by Pocketwatch: N mAh (P % of the gauge's full)" with how long it lasted, from which to which percent, at what average current; a toast when a run ends; too short a run says so and keeps nothing. | P1 | Implemented 2026-10-20 (ADR-042); host tests `battery_calibration_*`; HM30 pending | tests, HM30 |
| `BAT-REQ-011` | The calibration has a screen of its own (Home → Settings → Battery Calibration; System → Battery → Battery Calibration): the four steps (Charge, Unplug, Run down, Result) with the one now lit, what to do now, the charge and how the PSP is powered, the run's numbers (measuring: elapsed, mAh used, the average current, the full charge it projects once 30 % is used; after: the measure, how long it lasted, the average), the current, voltage, capacity and cycles, and the run's charge over its time as a chart (the last run's kept in `battery-last.txt`). Enter starts the run, or stops it after a question. | P1 | Implemented 2026-10-20 (owner: "a whole application in Settings for the calibration"); host test `battery_calibration_curve_*`; preview `home-calibration.png`; PPSSPP checked; HM30 pending | tests, preview, HM30 |

## C. Architecture and integration

The XMB plugin's thread (`vsh/src/xbattery.c`) adds a reading to the history every 15 s from the
XMB's start; opening Settings → Battery takes one more and builds the page's rows
(`vsh/src/xsettings.c`), each row with its reading as its only value, shown by the XMB's own list.
Without a current reading the estimate uses what the remaining capacity lost over two minutes.

Sources: scePower's user functions (`psppower.h`); syscon getters as in uOFW `include/syscon.h`
(`sceSysconBatteryGetElec/Cycle`, `int f(int *out)`), found with `sctrlHENFindFunction`
("sceSYSCON_Driver", ARK-4 translates the NIDs) and run with `kuKernelCall`. The current's sign:
negative while discharging (VERIFIED 2026-10-17); the estimates use its absolute value and the
charging flag. The first reading of every XMB start is logged (`xmb.log`: `xmb: battery ...`).

## D. User experience

Values right-aligned in the style's own list; the battery's readings under a heading "Measured by
the battery", so the system's percentage and the gauge's numbers are not confused.

## E. Reference material

| Reference | What | Use |
|---|---|---|
| uOFW `include/syscon.h` | syscon battery getters, signatures | adopt |
| PSPSDK `psppower.h` | scePower battery functions | adopt |
| ARK-4 `nid_660_data.c` | 6.60 NIDs of both | adopt (via `tools/nids/ark4_660.tsv`) |

## F. Implementation plan

1. ~~Model, readings, hub row, page~~ (2026-10-17).
2. B1 on the device: confirm the units and the sign of the current, the cycle count, behaviour
   with and without the charger.
3. Persist a compact history across sessions (charge sessions, average discharge per game).
4. The XMB page (needs a kernel helper for syscon from the VSH plugin).

## G. Testing and acceptance

Host: `make -C tests run T=battery`. Device: testing guide B1. Accepted when B1 shows plausible
values in every row (or "Not available" where the system has none) and the estimate settles within
five minutes of steady play.

## H. Risks and decisions

[ADR-022](../architecture/decisions.md#adr-022--battery-readings-from-the-batterys-own-gauge-with-labelled-estimates);
[RISK-005](../risk-register.md#risk-005--kernel-ram-and-prx-size-growth) (the code is in kernel RAM).
