# Complication framework — specification

| | |
|---|---|
| Area code | `CPL` |
| Status | Implemented 2026-10-17 ([ADR-009](../architecture/decisions.md#adr-009--complication-framework-and-ui-layering)): registry, item model, lifecycle; System, States, Settings and the debug card test are complications. Device test of the hub after the refactor pending ([testing guide](../testing.md#2-home-hub-and-the-look)) |
| Milestones | M4 (with the hub rewrite) |
| Code | [`complication.h`](../../plugin/src/complications/complication.h), [`registry.c`](../../plugin/src/complications/registry.c), [`table.c`](../../plugin/src/complications/table.c), [`system/`](../../plugin/src/complications/system/system.c), [`states/states_ui.c`](../../plugin/src/complications/states/states_ui.c), [`settings/`](../../plugin/src/complications/settings/settings.c); the hub: [`ui/hub.c`](../../plugin/src/ui/hub.c) |
| Tests | [`tests/test_registry.c`](../../tests/test_registry.c) |

## A. Overview

- **Purpose:** let a new feature (complication) add itself to the hub and receive lifecycle
  events without touching the engine, the shell or other complications (owner priority 3).
- **Scope:** registration, item model, lifecycle hooks, per-complication settings, build flags.
- **Non-goals:** a dynamic plugin loader (complications are compiled in).
- **Current state:** the hub draws whatever rows the table's complications give it and hands
  Enter back to the row's complication; it has no knowledge of states, settings or quitting.

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `CPL-REQ-001` | A complication is one folder plus one line in a registry table; nothing else changes to add it. | P1 | Implemented (`complications/table.c`) | review: the debug card test was added as one table line |
| `CPL-REQ-002` | The hub builds its items from the registry in registry order; each complication contributes items (action, sub-list, toggle, information). | P1 | Implemented (rows with icon, title, description, enabled; sub-lists are the complication's own screens) | `registry_collects_rows_in_table_order` |
| `CPL-REQ-003` | Lifecycle hooks: `init` (plugin start), `on_menu_open`, `on_menu_close`, `shutdown` (`module_stop`, no I/O). | P1 | Implemented: `init`, `on_open`, `changed` (polled every second), `shutdown` (reverse order), `startup_pending` / `startup` (once, when the game runs, ADR-020), `tick` (once a second from the shell while the hub is closed and the engine idle) and `on_quit` (Quit Game) - 2026-10-17, for Battery and Play Time; `on_menu_close` dropped (no user yet) | `registry_lifecycle_order_and_changes`, `registry_finds_a_pending_startup`, `registry_ticks_and_quits_those_that_ask` |
| `CPL-REQ-004` | Items can be disabled with a reason (e.g. "No state to load") and are shown disabled, not hidden. | P1 | Implemented: a disabled row still gets Enter so its complication can explain (the guards) | testing guide G1-G5 |
| `CPL-REQ-005` | System complication: Resume Game, Quit Game ([game shell](game-shell.md) `SHL-REQ-005`). | P0 | Done (device test 2026-10-16); two table entries so the others sit between them | device test M3a |
| `CPL-REQ-006` | Settings complication: per-complication settings through the config store (`SYS-REQ-010`). | P2 | Implemented: Fast Save and Load, Clock and Battery, Style, Language; edited in the XMB (Settings → Pocketwatch, ADR-024), read by the game at its start - the hub's Settings screen is gone | testing guide ST1-ST5 |
| `CPL-REQ-007` | Each complication can be left out at build time with a flag; the hub adapts. | P2 | Partial: `#ifdef` around its table line (the card test uses `PW_DEBUG`); no per-complication flags yet | release build has no card test |
| `CPL-NFR-001` | A complication costs nothing while the hub is closed beyond its static data; it creates no thread unless it needs one. | P1 | Implemented (no complication has a thread) | code review, `make check` |

## C. Architecture and integration

```c
/* complications/complication.h */
typedef struct {
    const char *id;
    void (*init)(void);                           /* plugin start, may do I/O */
    void (*shutdown)(void);                       /* module_stop, no I/O */
    void (*on_open)(void);                        /* the hub opens */
    int (*items)(pw_cpl_item *out, int max);      /* its rows (icon, title, desc, enabled) */
    int (*run)(int index, const pw_cpl_env *env); /* Enter: PW_CPL_STAY / CLOSE / QUIT / LOADED */
    int (*changed)(void);                         /* polled every second: rebuild the rows */
    int (*startup_pending)(void);                 /* once the game runs: something to do now? */
    int (*startup)(const pw_cpl_env *env);        /* then this, once, on the open screen */
    void (*tick)(unsigned int now_ms);            /* once a second, hub closed, engine idle */
    void (*on_quit)(void);                        /* Quit Game: the last card writes */
} pw_complication;
```

`run` executes in the hub's thread with the game frozen; `env` carries the UI (fonts, buttons)
and the frozen thread set for the engine. `PW_CPL_LOADED` tells the hub that a state was loaded:
the hub's screen buffers are gone and it returns at once. The table (current order): System
(Resume) · States (Save, Load) · Settings · System (Quit) · debug card test.

Plugin start runs core services first (log, engine, assets, language, game identity), then
`init` of every complication in table order; `module_stop` runs `shutdown` in reverse.

## D. User experience and visual design

Complications appear as rows in the hub ([UI spec](../ui/README.md)); their sub-screens (state
list, Settings) use the shared views. Order and icons follow the XMB look; hiding or reordering
by the user is future work ([ui/customization](../ui/customization.md)).

## E. Reference material

Custom Home's menu structure (behaviour only), the XMB's category/item model
([ui/references](../ui/references.md)).

## F. Implementation plan

Done: registry, table, System / States / Settings / card test as complications. Next: per
complication build flags (CPL-REQ-007) when a second optional complication exists; a category
bar if the owner picks hub concept B (ADR-016).

## G. Testing and acceptance

Host tests of the item model and registry iteration (`make -C tests run T=registry`); on the
device the hub must behave exactly as before the refactor (testing guide sections 2-7).

## H. Risks and decisions

[ADR-009](../architecture/decisions.md#adr-009--complication-framework-and-ui-layering). The
refactor of 2026-10-17 kept every flow of the device-tested hub; it is itself device-untested.
