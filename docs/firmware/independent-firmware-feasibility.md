# Independent PSP firmware — feasibility study

Question: could Pocketwatch's platform become independent of Sony's official firmware and of
existing custom firmware (ARK-4, PRO), and at what level does that pay off?
Status: **Research in progress** — gate FW-G1 (baseline) partly done, FW-G2..FW-G7 open. Track:
[roadmap § FW](../roadmap.md#fw--independent-firmware-research), decision record
[ADR-015](../architecture/decisions.md#adr-015--independent-firmware-is-a-gated-research-track) (Proposed).

> [!WARNING]
> This is a feasibility study, not a plan to build a firmware. Nothing here may lead to writing the
> NAND, the IPL or the SYSCON without an explicit owner decision recorded as an ADR and a tested
> recovery path on spare hardware ([RISK-012](../risk-register.md#risk-012--bricking-during-firmware-research)).

## 0. Scope and evidence quality

- "Independent software stack for PSP hardware" means our own code taking over roles that Sony's
  firmware plays today; it does **not** mean replacing the hardware.
- Labels: **VERIFIED** (a cited source supports it), **REPORTED** (stated by the 2026-10-16
  research pass with a source that was not captured in this document — must be re-checked in
  FW-G1/FW-G3), **INFERRED**, **SPECULATION**.
- The 2026-10-16 research pass ended early (the research request was stopped by the API's safety
  filter), so the boot and recovery sections are incomplete by design. This study deliberately
  stays at the level of capabilities and constraints; it does not describe exploitation techniques.

## 1. Hardware baseline (FW-G1, partial)

| Block | What is known | Evidence |
|---|---|---|
| CPU | Allegrex (MIPS32R2-based), 33-333 MHz, FPU + VFPU, an MPU but no TLB → no virtual memory; only uClinux-style kernels are possible | VERIFIED: [Copetti, PSP architecture](https://www.copetti.org/writings/consoles/playstation-portable/), [linux-mips wiki: PSP](https://linux-mips.org/wiki/PSP) |
| Media Engine | second Allegrex core with 2 MB eDRAM, H.264 decoder, a DSP ("VME") still largely undocumented | VERIFIED: Copetti |
| Memory | 32 MB RAM (64 MB on PSP-2000/3000), 2 MB VRAM (eDRAM), 16 KB scratchpad | VERIFIED: [PPSSPP MemMap.h](https://github.com/hrydgard/ppsspp/blob/master/Core/MemMap.h); our own M1 measurements ([m1-hardware](../research/m1-hardware.md)) |
| KIRK crypto engine | AES/ECDSA/SHA-1/RNG; its ROM has been dumped and reimplemented in C | VERIFIED: [uOFW kirk-rom.c](https://github.com/uofw/uofw/blob/master/src/kirk/kirk-rom.c) |
| SYSCON (Baryon) | Renesas 78K0 on SPI; power, buttons, battery, LEDs; firmware dumped and partly reversed (TA-096); newer Baryons need a handshake before accepting commands | VERIFIED: [uOFW syscon firmware](https://github.com/uofw/uofw/blob/master/src/syscon_firmware/firmware_ta096.c), [pysweeper](https://github.com/khubik2/pysweeper), [iplsdk](https://github.com/DaveeFTW/iplsdk) |
| Bare-metal drivers | iplsdk (MIT) provides DDR init, NAND with the per-console scramble and a translation layer, Memory Stick Pro read/write, LCD controller and panel types, vblank interrupts, syscon (buttons, LEDs, power), KIRK, UART including the headphone-remote UART | REPORTED (from the iplsdk repository, to re-check file by file) |
| Only as reimplementations of Sony's modules | GE (graphics) driver, audio, the power/suspend path | INFERRED from uOFW ([uofw](https://github.com/uofw/uofw)); GE command lists themselves are well documented by PPSSPP and the pspsdk |
| No public bare-metal driver | USB, Wi-Fi, UMD drive, Media Engine firmware/codecs, suspend/resume | REPORTED |
| Always Sony code, whatever we do | the mask-ROM pre-IPL, the Baryon firmware, the Wi-Fi chip firmware, the UMD controller (Lepton) firmware | REPORTED; INFERRED from the architecture (these run on separate processors or ROM) |

Gaps: register-level documentation of the display/GE/audio blocks beyond what drivers imply; the
exact PSP-3000 board revisions of the owner's unit (TA-090/092/093/095 — to read on the device);
the current state of IPL-level code execution and service-mode recovery per board, which the
research pass reported as now possible on all retail models (REPORTED, high-impact, **must be
re-verified with primary sources before any decision**).

## 2. Existing software and tools (FW-G2, to do)

| Item | Role | Status of evidence |
|---|---|---|
| uOFW | open reimplementation of 6.60 kernel modules (sysmem, loadcore, ctrl, power, display, ge, audio, ...) | VERIFIED exists; completeness per module and whether modules run on hardware: RESEARCH REQUIRED |
| iplsdk (Davee) | SDK for code running at IPL time, bare-metal drivers | REPORTED (MIT) |
| PSP Linux (uClinux port) | demonstrated non-Sony kernel on PSP hardware (older models), started from a homebrew loader after Sony's boot | INFERRED from [linux-mips wiki](https://linux-mips.org/wiki/PSP); driver coverage RESEARCH REQUIRED |
| pspdev toolchain | GCC, pspsdk; our current toolchain | VERIFIED (in use) |
| PPSSPP | high-level emulator: excellent for user-mode behaviour, not for boot/driver work | VERIFIED (in use for research) |
| Baryon Sweeper / pysweeper | service-mode recovery tooling | REPORTED: works on all consumer revisions |
| ARK-4, PRO | custom firmware built on Sony's kernel (comparison points) | VERIFIED (we run on ARK-4) |

## 3. Implementation levels compared

Complexity is a qualitative engineering estimate (INFERRED/SPECULATION) for one experienced
developer with the owner's device time, not a schedule.

| | A. Own interface / launcher | B. Own application environment | C. Own system layer | D. Own firmware on PSP hardware | E. New stack from scratch |
|---|---|---|---|---|---|
| **Independent** | the whole user-facing experience (hub, launcher, XMB-like shell) | app model, UI toolkit, services, app lifecycle | selected kernel modules (e.g. display, ctrl, power, our own FS cache) | the kernel and drivers; boots without Sony's kernel and without ARK/PRO | everything above the mask ROM and the side processors |
| **Still depends on** | Sony kernel + drivers + ARK-4 | Sony kernel + drivers (syscalls), CFW to run unsigned code | the remaining Sony modules, CFW loader | pre-IPL, Baryon/Wi-Fi/UMD firmware; KIRK as hardware | same as D |
| **Already demonstrated** | yes: Custom Home, ARK launchers, VSH menus, Pocketwatch's hub (2026-10-16) | yes: large homebrew (emulators, shells, PSPLink) | partly: ARK/PRO patch and replace modules; uOFW modules exist (hardware use RESEARCH REQUIRED) | partly: IPL-time code and drivers (iplsdk, REPORTED); PSP Linux on older models | minimal demos only (boot → LCD → buttons → MS, REPORTED via iplsdk) |
| **Unknown / reverse engineering needed** | nothing fundamental | little | each module's hidden contracts with the rest of the kernel | GE/audio/power without Sony drivers; USB, Wi-Fi, UMD, ME, suspend; running retail games would need the whole Sony API (uOFW + HLE) | all of D, plus own bootstrap and recovery |
| **Major blockers** | none | none | compatibility: one wrong module bricks the boot (mitigated by loading from the Memory Stick via the CFW, never flashing) | game compatibility (practically unreachable), USB/Wi-Fi/ME, per-board differences, IPL-level brick risk | as D, without any reuse |
| **Tooling / test hardware** | what we have | what we have | what we have + a second PSP for risky tests | spare PSP-3000 board(s), service-mode recovery tooling, UART adapter, possibly a hardware programmer | as D |
| **Reliability / recovery** | excellent: plugin off = stock CFW | excellent | good if nothing is written to flash | depends on service-mode recovery on that exact board | as D |
| **Complexity (qualitative)** | low-medium: weeks per major screen | medium: months | medium-high per module | very high for a useful OS; extreme for game compatibility | extreme |
| **Advantages** | directly serves the owner's priorities (native look, speed, extension) | full control of an app platform | removes specific firmware limits (e.g. faster suspend, FS cache behaviour) | true control of the hardware, no CFW dependency | maximum independence |
| **Disadvantages / opportunity cost** | depends on ARK-4 and 6.61 | still needs CFW; no game-level features beyond A | high review/test cost; fragile across firmware updates (none expected for 6.61) | loses every retail game (unless D reimplements Sony's kernel), years of work, brick risk | loses everything existing |
| **Measurable proof of concept** | M4 hub accepted by the owner side by side with the XMB | an app launched from our environment with its own UI and storage | one Sony module replaced by ours, loaded from the Memory Stick, with a 100-cycle test passing | on a spare board: own payload boots, initialises the LCD, draws text, reads buttons and the Memory Stick, then hands over to the normal boot; zero NAND writes | same PoC without iplsdk |

## 4. Findings so far

1. **(INFERRED)** For Pocketwatch's goals (native-feeling features inside retail games), only
   levels A-C keep the games; D and E replace the very firmware the games need. Independence
   pays off for *system* features (boot speed, own launcher, suspend control), not for in-game
   complications.
2. **(REPORTED, to verify)** Low-level execution and recovery are reported to be possible on all
   retail models today, and MIT-licensed bare-metal drivers exist for the core blocks; if true,
   a level-D *research PoC* is technically reachable on spare hardware.
3. **(INFERRED)** Never fully independent: pre-IPL ROM and side-processor firmware always run.
4. **(INFERRED)** The cheapest step towards "more control" with real product value is level C for
   specific services, loaded from the Memory Stick through ARK-4 (no flashing), e.g. a faster
   suspend path (backlog S1/S2) or a FAT cache policy that makes E3 safe.

## 5. Legal and licensing considerations (not legal advice)

- Sony's modules, keys and signed images must never be redistributed; our code must not embed
  keys (Pocketwatch already uses the firmware's own chnnlsv/KIRK paths for hashing).
- Clean-room style work: study behaviour and documentation, write our own code; reuse only code
  whose license allows it (uOFW: license to confirm before reuse, RESEARCH REQUIRED; iplsdk: MIT
  REPORTED; ARK-4: GPL-3.0).
- Anti-circumvention law differs by country; any work that touches boot-time code signing should
  be reviewed against local law before publication.

## 6. Research gates and next actions

| Gate | Next action | Exit criterion | State |
|---|---|---|---|
| FW-G1 Baseline | log the owner's board revision (Tachyon/Baryon/Pommel versions, `sceKernelGetModel`, motherboard via syscon getters) from the debug probe; re-verify §1 rows marked REPORTED against primary sources | table in §1 fully VERIFIED or marked unknown | In Progress |
| FW-G2 Docs and tools | audit uOFW module completeness, iplsdk drivers, PSP Linux driver coverage, licenses | §2 table complete with licenses | Planned |
| FW-G3 Boot constraints | from primary sources only: which boot entry points and recovery options exist for the owner's exact board | written constraints + recovery path, reviewed by the owner | Planned |
| FW-G4 Dependency matrix | for options C and D list which Sony modules/firmwares remain and which models each reaches | matrix | Planned |
| FW-G5 Proof of concept | pick the most uncertain assumption (likely: level-C module replacement loaded from the card) and run its PoC; any level-D PoC only on spare hardware with tested recovery | measurable result in a research note | Planned |
| FW-G6 Comparison | score A-E on independence gained vs cost and risk with the PoC data | table + recommendation | Planned |
| FW-G7 Go / no-go | owner decision | ADR | Planned |

**Recommendation so far (INFERRED):** keep the product on level A (Pocketwatch on ARK-4) and
consider level C for specific services where the firmware is the bottleneck; run FW-G1/FW-G2 as
desk research between milestones; do not start any level-D work before FW-G3 is answered from
primary sources and spare hardware exists.
