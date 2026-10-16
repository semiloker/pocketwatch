# UI customization — architecture

Status: **PROPOSED** (nothing implemented). Principle: the first and strongest customization is
*following the system*: whatever the user set in the XMB (language, Enter button, theme colour,
custom XMB theme files) applies to Pocketwatch automatically. Pocketwatch's own settings stay few
and only cover what the system has no setting for.

## 1. What can be customized

| Level | Item | Priority | Mechanism |
|---|---|---|---|
| Follow the system | Language | P1 | `sceImposeGetLanguageMode` at hub open |
| Follow the system | Enter/Back button (○/×) | P0 | same call (`button` out-parameter) |
| Follow the system | Theme colour / month background | P2 | `01-12*.bmp` / `13-27.bmp` per the XMB setting (RESEARCH REQUIRED: where the chosen colour is stored) |
| Follow the system | Custom XMB themes (CXMB, redirected RCOs) | P2 | read whatever `topmenu_icon.rco` / `system_plugin_fg.rco` resolve to; zlib images via firmware deflate |
| Pocketwatch setting | Hub layout: item column (A) / cross (B) | P2 | config key `ui.layout` |
| Pocketwatch setting | Item visibility and order (e.g. hide Quit Game) | P2 | `ui.items` list |
| Pocketwatch setting | Background dim level | P3 | `ui.dim` 0-100 |
| Pocketwatch setting | Clock/battery corner on/off | P3 | `ui.status_corner` |
| Pocketwatch setting | Animations on/off | P3 | `ui.motion` |
| Complication settings | States: fast mode, confirm overwrite, slots per game | P2 | `states.*` keys |
| Not planned | Own skin format, own fonts | — | conflicts with "native 1:1"; revisit only on request |

## 2. Configuration format and persistence

- **File:** `ms0:/seplugins/pocketwatch/config.txt` (next to the PRX; `ef0:` on a PSP Go),
  plain text, one `key = value` per line, `#` comments, UTF-8, ≤ 4 KB. Same spirit as ARK-4's
  `SETTINGS.TXT`, editable on a PC.
- **Keys** are namespaced per complication (`ui.*`, `states.*`, `shell.*`); a `version = 1` line
  allows migrations.
- **Reading:** once at plugin start (no state can be held yet) into a fixed table; never in a
  suspend path.
- **Writing:** only from the Settings screen, only when no state is held
  (`SYS-REQ-012` write gate), whole-file rewrite to a temporary name then rename.

## 3. Runtime application and validation

- Each key has a type (bool, int range, enum, list), a default and a validator in one table
  owned by the complication that declares it ([complications](../subsystems/complications.md)).
- Invalid or unknown values are ignored with one log line; the default applies.
- Changes from the Settings screen apply on the next hub open (no live re-layout in v0.1).

## 4. Defaults and fallback

Defaults reproduce the native behaviour exactly (layout A, all items visible, system dim level,
no status corner, motion on, fast mode on, confirm overwrite on). A missing file means defaults; a
damaged file means defaults plus a message in Settings ("Settings were reset").

## 5. Compatibility and migration

`version` bumps only when a key changes meaning; old keys are read once and rewritten in the new
form; unknown keys are preserved on rewrite so a newer config survives an older build.

## 6. Security and stability

The parser is portable, bounded (line length, number of keys), host-tested with fuzzed input,
and never allocates; a config can never crash the kernel or change engine safety rules (no keys
for the flash rebuild, the write gate or memory locations).

## 7. Plugin-driven UI extensions

Complications extend the UI only through the registry's item model (actions, lists, toggles,
information rows) and the shared widgets; they do not draw directly. This keeps every screen
consistent with the theme and lets the layout setting apply to all of them.

## 8. Acceptance (when implemented)

| Criterion | Test |
|---|---|
| Changing the XMB language changes the hub language without touching Pocketwatch settings | device |
| Swapping Enter (ARK VSH menu) swaps Enter/Back in the hub and its hints | device |
| A config with garbage lines loads defaults and logs them | host fuzz test |
| Settings writes never happen while a state is held | code review + debug assertion |
