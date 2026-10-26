# Research — the in-game HOME freeze and Home's performance (2026-10-25)

Owner's report (device, build `fcab0aa`→`d3795f0`): pressing HOME in a game froze the game for
good ("full frozen, nothing happens, need to reboot"); with RemoteJoyLite off it "blurred, then
froze". Also: occasional micro-freezes, and music/photo covers "very long loading". Related
requests: five brightness levels (not four), a start-up animation.

## 1. In-game HOME freeze — root cause

**Symptom chain (VERIFIED, device + logs):**
- `pocketwatch.log` shows the plugin loaded, took over HOME at game start (`shell: HOME opens
  Pocketwatch`), then **nothing** after the HOME press — no `shell: open:` line. The hub's log is
  deferred while the game is frozen and is flushed on resume, so its absence means the hub never
  resumed: a hard hang, not the ADR-032 card-busy path (that resumes after 2 s).
- RemoteJoyLite OFF → still freezes, and now the screen **blurs then freezes**. The blur is the
  hub's own backdrop (it blurs the frozen game frame), so the hub *does* start rendering, then the
  system hangs and needs a reboot. So it is neither RemoteJoyLite nor the card-busy path.

**Root cause (VERIFIED by inspection, `perf/src/main.c` `perf_thread`):** the performance overlay
is a kernel thread that, every vblank, drew the FPS/OSD **onto whatever framebuffer is currently
displayed** when it differs from the game's own buffer:

```c
if (... && top != game) {           /* "the hub's frame": draw onto it */
    void *t = (void *)((unsigned int)top | 0x40000000u);
    lines_draw(t, bufw, fmt);
    osd_draw(t, bufw, fmt, now, 1);
}
```

When the hub opens it displays its **own** framebuffer (in the plugin's kernel RAM). `top != game`
then, so the perf thread wrote into a buffer it does not own, with a width/stride it only guessed
(`bufw`/`fmt` from the display) — a kernel write to another module's memory. That faults, and a
fault in a kernel thread hangs the PSP: **blur renders (hub backdrop), next vblank the perf thread
faults on the hub's buffer, freeze + reboot.** The overlay module is new since ADR-032, which is
why HOME worked on 2026-10-16 and regressed.

**Fix (ADR-051):** the overlay only ever writes to the game's **own** buffers, and only while the
game is the one on screen (`top == game`). When the hub (or any overlay) owns the screen, it draws
nothing. The FPS pill no longer shows over the hub (a menu does not need it), and the dangerous
write to a foreign buffer is gone. Mitigation already on the owner's card: `Overlay in Games` set
off in PLUGINS.TXT until the fix is confirmed.

**Round 11b (owner, same day):** the first fix drew only when `top == game`, which on the device
made the overlay **invisible** — the owner's games display from a back buffer, so `top != game`
during normal play as well, not only when the hub is open. The reliable signal is the **memory
region**: a game framebuffer is in VRAM (`0x04000000-0x041FFFFF`); the hub's page is in a kernel
partition. The overlay now tracks and draws onto **only VRAM buffers** (`is_vram`) and never reads
the displayed buffer — the hub's page can never match, so it is never written.

Also round 11b: with the overlay on, **quitting a game black-screened and powered the PSP off**.
`module_stop` waited for the overlay thread with no timeout, but the thread blocks in
`sceDisplayWaitVblankStart`; during the exit the display is torn down, vblank stops, the thread
never ends, `module_stop` hangs the exit, and the firmware powers the PSP off. Fixed: `module_stop`
waits ~120 ms then `sceKernelTerminateDeleteThread`.

**Round 11c (owner, 2026-10-26):** the VRAM filter worked — the overlay **shows in games and HOME
no longer freezes**. But quitting a game to the XMB **still** black-screened and powered the PSP
off, so the round 11b `module_stop` fix (timeout + `sceKernelTerminateDeleteThread`) did **not**
address it.

Root cause (INFERRED, device test pending): round 11b made the overlay draw onto **every** tracked
VRAM buffer **every vblank regardless of what is on screen** (so it is ready when the game returns).
Quitting runs with the hub on screen and the game tearing its display down to reboot to the XMB
(`plugin/src/shell/shell.c` `quit_game` → the game's exit callback or `sctrlKernelExitVSH`);
throughout that window the perf thread keeps writing the game's VRAM — which `reboot.bin` loads
into — corrupting it, so the reboot fails and the firmware powers off. `module_stop` is the wrong
hook (its terminate did nothing to the power-off; the writes happen in the draw loop, and
force-terminating a thread inside `sceDisplayWaitVblankStart` is itself unsafe).

Fix attempted (ADR-053, first version): draw only while a **VRAM buffer is the one on screen** —
read the displayed buffer each vblank (`sceDisplayGetFrameBufferInternal`) and skip drawing unless
`is_vram(displayed)`; `module_stop` no longer force-terminates. **Both parts were wrong — see round
11d.**

## 1d. The real root cause — the thread blocks in the display driver (VERIFIED, round 11d)

**Round 11d (owner, 2026-10-26, log attached):** the round-11c build **still** black-screened and
powered off on quit, and the overlay was now **invisible** in game. Two findings from
`pocketwatch.log` and the code falsify the round-11c theory:

- **The power-off is independent of drawing.** The last logged session ran the 11c build, whose
  overlay drew nothing (the displayed-buffer gate always skipped) — `cpu: hub ... PocketwatchPerf
  0%` — and it **still** powered off on quit. So the cause is not a framebuffer write during the
  reboot (the round-11c theory). Every quit ends at `shell: quit: the game's exit callback was
  invoked (0x00000000)` with all eight IPL seeds present (not the seed hang, m2-speed), then the
  reboot dies.
- **Reading the displayed buffer hides the overlay.** `sceDisplayGetFrameBufferInternal` did not
  return an address that passed `is_vram` on this device, so the gate skipped every frame. The
  overlay was visible only in round 11b, the one build that did **not** read the displayed buffer;
  it vanished in 11a and 11c, the two that did.

**Root cause (VERIFIED, log + code):** the overlay thread paced on `sceDisplayWaitVblankStart`, a
display-driver call, and is the only Pocketwatch thread that blocks in a driver (the shell, main and
probe threads all block in `sceKernelDelayThread`). On the game's exit the kernel unloads the module
(`module_stop` runs — the game plugin's does, and ARK unloads them together), but the overlay thread
is stuck inside the display driver as that driver is torn down for the reboot, so it cannot be woken
and **outlives `module_stop`**. The plugin's own `module_stop` names this exact hazard
(`plugin/src/core/main.c`: "No thread of ours may outlive this function: a thread left running in
unloaded memory hangs the exit"). A kernel thread alive when `reboot.bin` reloads the kernel → fault
→ power off. This is why the ADR-051/11c `module_stop` edits (terminate, then no-terminate) changed
nothing: the thread is unstoppable while it sits in the display driver either way.

**Fix (ADR-053, corrected):** the overlay thread **paces on `sceKernelDelayThread` (~15 ms), never
`sceDisplayWaitVblankStart`**, and re-checks `g_run` right after the delay — the same shape as every
other Pocketwatch thread. It is then never blocked in a driver, so `module_stop` ends it within a
frame, before the reboot. The displayed-buffer gate is removed: the overlay writes onto every
tracked VRAM buffer (ADR-051), visible in play and off-screen while the hub is open. FPS still comes
from counting buffer flips, which a 15 ms loop catches.

**Device test (pending, PF-FREEZE):** with the corrected `pocketwatch_perf.prx` and `Overlay in
Games` on, launch a game — the FPS pill shows **in the game**; press HOME — the hub opens, no pill
over it, no freeze; **Quit Game — the XMB comes up** (no black screen/power-off). RemoteJoyLite has
been removed from the card, so test as-is.

## 2. Performance from `home.log` (VERIFIED)

- **Covers:** `home cover: 1200x1200 (tag), read and decoded 1175 ms, all 1924 ms` (and 1340/2437
  ms). The owner's music has 1200×1200 album art in its ID3 tags; stb_image decodes it at full
  resolution (~1.2 s on the 333 MHz CPU). This runs on the **loader thread** (`library.c`
  `loader`/`load_cover`), not the frame loop, so it does not stall the UI — it is the cover
  *appearing* ~2 s late. A placeholder (the note square) shows meanwhile. **Proper fix is a
  separate task:** a downscaled decode (stb has no scaled decode; the PSP's `sceJpeg` can decode to
  a target size) or a per-track cover cache. Tracked in the backlog.
- **Micro-freezes:** the `slow:` frames are `[1885 ms p1 keys 1862]` (p1 = `P_GAME`: launching a
  game, expected), `[1068 ms p11 keys 1049]` (p11 = `P_SETTINGS`: applying a setting), and
  `[~750 ms p7 state ~730]` (p7 = `P_NOW`: Now Playing waiting on its cover). These are heavy
  one-off actions, not a per-frame regression; steady state is 52-58 fps.

## 3. Brightness levels (VERIFIED fix)

`home/src/hsettings.c` hard-coded four levels (`get_bright` rejected `v >= 4`, the choice used
`four`), so the device's fifth backlight level was unreachable. Now 0-4 (`BRIGHT_MAX`), five
choices, and the slider/text scale to `hm_bright_max()`; the `set_bright` verify guard makes an
unsupported level a no-op on four-level units.

## 4. Start-up animation (ADR-052)

A brief wordmark overlay (`hm_view.intro`, drawn in `hm_draw`): the watch mark, "Pocketwatch" and
"Pause. Save. Play." hold ~650 ms then fade out ~450 ms to the launcher; any button skips to the
fade-out. Reference `t_boot` captured at the loop's start (not `t_open`, which can reset
mid-session). Preview `home-intro`.
