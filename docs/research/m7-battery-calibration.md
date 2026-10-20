# M7 — calibrating the battery and saying how long it lasts

Question: how can Pocketwatch "calibrate" the PSP's battery so that its charge and time left can be
trusted, and show its data in a way that is understood (owner, 2026-10-20)? Status: Open
(2026-10-20) - implemented in Home (ADR-042); device test HM30 pending.

## Background and sources

**What the PSP reports.** The system's percentage, charging, external power, its own estimate of
the time left, the voltage, the temperature, the remaining and the full capacity (mAh) come from
scePower; the battery's own gauge answers the syscon with the current and the charge cycles.
**VERIFIED** (the owner's PSP-3000, 2026-10-17: 4098 mV, -224 mA discharging, 20 C, full 1790
mAh, 0 cycles; [battery spec](../subsystems/battery.md), BAT-REQ-001). The current is negative
while discharging (VERIFIED, same reading). The cycle count and the current are commands the syscon
sends to the battery: ~35 ms each (`home.log` 2026-10-19 and 2026-10-20: `cycles 35647 us`).

**Where the percentage comes from.** The battery has its own gauge (a chip in the pack that counts
the charge in and out and keeps a learnt full capacity; **INFERRED** from the readings and from how
such gauges work - Sony does not document the pack). The system's percentage is not simply
remaining / full: the owner's PSP showed 93 % with 1513 of 1790 mAh (85 %). **VERIFIED**
(`xmb.log` 2026-10-20: `battery 93% ... 1513/1790 mAh`). That gap is one reason a measurement of
our own is worth having.

**What "calibrating" can mean.** (1) The gauge's own learning: a full charge, then a run down to
the PSP turning itself off, then a full charge again lets a gauge re-learn the full capacity
(**INFERRED**, the usual behaviour of coulomb-counting gauges; not measured on a PSP pack).
(2) A measurement of our own: during that same run Pocketwatch integrates the current it reads and
so knows the capacity the battery really delivered, independently of the gauge's figure.
Writing the gauge or the pack's memory is not an option: that is the service-mode ("Pandora")
territory of the battery and out of bounds (AI workflow, section 9: nothing that writes the
SYSCON's or the battery's own firmware or memory without an owner decision and a recovery path).

**Sampling.** The current every 10 s (a syscon command each time, from Home's battery thread, not
the frame loop: ADR-031); a sample written every 30 s - a run of 5-6 hours is ~700 lines (~25 KB).
The PSP must not sleep on its own in the middle: `scePowerTick(PSP_POWER_TICK_SUSPEND)` while the
run lasts (the screen may still dim or go off with HOLD, which saves the battery as in use).

**The run's end.** The PSP turns itself off at the bottom of the charge; the run's file stays on
the card and the next start of Home finishes it: the samples are read again, the capacity
computed and kept (`battery.txt`). A gap of more than 120 s between samples (the PSP slept, Home
was left for a game) is not counted, and neither is the charge lost in it, so a run with gaps
still scales correctly (`hm_battcal_parse`, host test `battery_calibration_*`).

## Options

| | What | Result |
|---|---|---|
| A | Show the gauge's numbers clearly (current, remaining / full, ours "about" estimate) | what the battery says, now |
| B | A guided run from full to empty, measured by Pocketwatch | what the battery really gives, once per run; may help the gauge re-learn |
| C | Learn a voltage-to-charge curve | needs many runs; the voltage drops under load: not now |
| D | Write the gauge | out of bounds |

Chosen (ADR-042): A and B.

## Procedure

**B1-cal - one run (HM30).** Charge the PSP until the light goes off (full). In Home → System →
Battery → Calibration → Start: "Full: unplug the charger"; unplug; "Measuring: leave Home on".
Leave Home on (music, photos, or nothing) until the PSP turns itself off. Charge it, start it:
Home's toast "Battery measured: N mAh"; the Battery page: Measured by Pocketwatch "N mAh (P % of
F)", Lasted "5 h 10 min   100% - 3%   320 mA". Send `home.log` (`home battery:` lines) and
`seplugins/pocketwatch/battery.txt`.

## Results

None yet.

## Conclusions

- The time left is shown from our own history (the run's average current), "about", the system's
  figure before there is any; the current is shown as measured.
- OPEN QUESTION (device): how close the measured capacity is to the gauge's full capacity on the
  owner's battery, and whether the gauge's full changes after a run (compare the Capacity row
  before and after HM30).
- Next: HM30.
