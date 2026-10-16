# Backlog

Work that is understood but not scheduled yet. Each item says what it is worth and where the
evidence is, so it can be picked up cold.

## Speed (savestate engine)

State of 2026-10-16 (Castlevania, PSP-3000, fast mode, LZ4): first save of a session 7.8 s (real
sleep), later saves 4.2 s, loads 3.9 s. Measurements and history: `docs/research/m2-speed.md`.

| # | Idea | Expected gain | Notes |
|---|---|---|---|
| S1 | **Find the slow suspend handlers.** Events `0x200` (~560 ms) and `0x400A` (583 ms) before the sleep point, `0x100000` (269 ms) and `0x1000A` (122 ms) after it. | up to ~1 s per save and per load | Profile per handler: walk the sysevent handler list (`sceKernelReferSysEventHandler`) and time each handler, e.g. by wrapping its function pointer for one diagnostic suspend. Measure first, change nothing. Candidates: a driver's fixed delay (display/LCD, audio fade, UMD/Inferno) or a plugin (bright3, battery_icon, RemoteJoyLite, cwcheat). |
| S2 | **Overlap packing/unpacking with the suspend.** The power thread sleeps between `0x401/0x402` retries (10 ms x ~30) and some handlers wait; our thread could pack game RAM then. Unpack could start as soon as the rolled-back thread runs, during the resume phases. | up to ~1 s | eDRAM must be packed before PHASE0_3 (the GE turns its bus clock off) and unpacked after the GE resume; game RAM is safe while the game stays frozen. |
| S3 | **Skip the flash driver rebuild on load when the NAND did not change.** | 0.8 s per load | Needs a reliable "NAND untouched since the save" signal (e.g. lflash/flashfat module data unchanged). Never skip it blindly: a stale lflash map corrupts flash1 (CLAUDE.md). |
| S4 | **Drop the real sleep on the first save of a session.** It only exists to capture a real wake's resume info (battery/UMD/unknown bits). The exit hang was the IPL seeds, not the resume info. | ~3.6 s once per session | Test: touch-and-go with live getters only, check battery icon, Memory Stick, Wi-Fi switch, quitting. |
| S5 | **Cheaper load verification.** The arena + image checksum before the freeze costs ~0.23 s. | ~0.2 s | Keep the image guard at PHASE0_0; verify the arena per record while decompressing instead. |
| S6 | **Find the `0x401/0x402` veto** (~22-30 retries, ~0.34 s). | ~0.3 s | Dispatch `0x402` ourselves with break-on-error and log the handler name; bisect GAME plugins. Never bypass kernel power locks (NAND writes). |
| S7 | **Hand-written LZ4 decoder** (port libdragon `lz4_dec_fast.S`, Unlicense). | maybe 2x on 0.45 s unpack | Only if C LZ4 stays below ~80 MB/s. |

Rejected (see m2-speed.md): Media Engine as a second CPU, raw PLL overclock beyond ARK's
333/166, bypassing power locks, our own suspend dispatch.

## Open questions

- The first touch-and-go save right after a real-sleep save once never completed (speed run 6;
  not seen again in run 7). Watch the debug log for a save without "save OK".
