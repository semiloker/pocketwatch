# Glossary

| Term | Meaning in this project |
|---|---|
| **Complication** | One Pocketwatch feature (watchmaking term for anything beyond telling the time). States is the first. |
| **States** | The savestate complication: save, load, delete the complete state of a running game. |
| **State** | One saved moment: game RAM, eDRAM, scratchpad, kernel image, P8, ME eDRAM save, plus a header. |
| **Hub** | The in-game Pocketwatch menu (target: XMB style, opened with HOME). |
| **Shell (in-game shell)** | What turns a button press into an open hub: trigger, freeze, display, button masking, guards, Quit Game. |
| **Freeze / thaw** | Suspend / resume every user-mode thread (attribute `0x80000000`) of the game ([`core/freeze.c`](../plugin/src/core/freeze.c)). |
| **Held freeze** | The caller (the menu) froze the game and keeps it frozen across an engine save/load (`pw_engine_*_held`). |
| **Capture** | Copying the 4 MB kernel image (+ P8, ME save) into P9 at PHASE0_0 of a save's suspend. |
| **Rollback** | A load: copying the captured kernel image back at PHASE0_0 of the load's suspend, so execution continues in the save's suspend. |
| **Apply blob** | The small copy-and-return routine (`handoff.S`) that performs the rollback from P9. |
| **Real sleep / touch-and-go (T&G)** | A system suspend that powers down and is woken by the RTC alarm / one that runs every suspend and resume event without sleeping (`scePowerRequestSuspendTouchAndGo`). |
| **Fast mode** | Engine setting: use touch-and-go after the first real sleep of a boot (default on). |
| **IPL seeds** | 8 words at `0xBFC00200` that the next reboot needs to decrypt reboot.bin; touch-and-go zeroes them. |
| **Resume info** | The firmware's `ScePowerResumeInfo` (wake reason, battery, switches); touch-and-go zeroes it. |
| **PHASE0_0** | Suspend sysevent `0x4000`, the last point where the kernel is complete and drivers have saved their state. |
| **Sysevent** | Kernel suspend/resume notifications (`sceKernelRegisterSysEventHandler`). |
| **P1/P2/P4/P5/P8/P9/P11** | Memory partitions: P1+P4 kernel, P2 game (24 MB), P5 volatile, P8 system 1 MB, **P9 24 MB extra RAM (state data)**, P11 4 MB extra RAM (ME save area during a suspend). |
| **eDRAM / VRAM** | The GE's 2 MB embedded DRAM at `0x04000000`. |
| **ME** | Media Engine, the second Allegrex core (audio/video); its eDRAM is saved to P11 during a suspend. |
| **GE** | Graphics Engine. |
| **lflash / flashfat** | NAND translation layer and FAT driver for flash0:/flash1:; rebuilt after every resume. |
| **ARK-4** | The custom firmware the plugin runs on (6.61); provides SystemControl (`sctrl*`), the NID resolver, Inferno (ISO driver). |
| **NID** | 32-bit function identifier in PSP import/export tables; 6.xx randomizes kernel NIDs, ARK-4 maps old ones. |
| **OFW / CFW** | Official firmware / custom firmware. |
| **XMB** | XrossMediaBar, the PSP's system menu. |
| **SAVEDATA folder** | `ms0:/PSP/SAVEDATA/<dir>/` with PARAM.SFO, ICON0.PNG, PIC1.PNG and data files, listed by the XMB's Saved Data Utility. |
| **PARAM.SFO / SFO hash** | Save metadata file; the XMB checks its MAC (mode 0x01 via chnnlsv.prx). |
| **PGF** | Sony's bitmap font format (`flash0:/font/*.pgf`). |
| **RCO / GIM** | Sony's resource container (`flash0:/vsh/resource/*.rco`) and its image format. |
| **Impose** | The firmware module behind the HOME popup, volume and brightness overlays. |
| **Guard** | A check that refuses an operation at an unsafe moment (USB on, low battery, game dialog open, ...). |
| **Fingerprint** | (E3) A hash of everything outside the kernel image that must match for a cross-boot load. |
| **E1..E4** | Engine ladder rungs: capture, same-boot load, cross-boot load, file-system integrity gate. |
| **T0..T7** | E3 device tests ([m3-menu-e3](research/m3-menu-e3.md)). |
| **S1..S7** | Speed backlog items ([backlog](backlog.md)). |
| **M0..M7** | Roadmap milestones ([roadmap](roadmap.md)). |
| **FW-G1..FW-G7** | Firmware research gates ([feasibility study](firmware/independent-firmware-feasibility.md)). |
| **Debug build** | `DEBUG=1`: probe combos, benchmark, extra checks. Release: `DEBUG=0`. |
