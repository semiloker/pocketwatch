# Complication framework — specification

| | |
|---|---|
| Area code | `CPL` |
| Status | Planned ([ADR-009](../architecture/decisions.md#adr-009--complication-framework-and-ui-layering)); today the phase-1 menu hard-codes its items |
| Milestones | M4 (with the hub rewrite) |
| Code | — (future `plugin/src/complications/registry.c`, `complications/system/`, `complications/settings/`) |

## A. Overview

- **Purpose:** let a new feature (complication) add itself to the hub and receive lifecycle
  events without touching the engine, the shell or other complications (owner priority 3).
- **Scope:** registration, item model, lifecycle hooks, per-complication settings, build flags.
- **Non-goals:** a dynamic plugin loader (complications are compiled in).
- **Current state:** none; `ui/menu.c` lists Save/Load/Resume directly.
- **Target state:** States, System (Resume, Quit Game) and Settings are complications.

## B. Requirements

| ID | Requirement | Priority | Status | Verification |
|---|---|---|---|---|
| `CPL-REQ-001` | A complication is one folder plus one line in a registry table; nothing else changes to add it. | P1 | Planned | review: adding a test complication touches only its folder and the table |
| `CPL-REQ-002` | The hub builds its items from the registry in registry order; each complication contributes items (action, sub-list, toggle, information). | P1 | Planned | host test of the item model |
| `CPL-REQ-003` | Lifecycle hooks: `init` (plugin start), `on_menu_open`, `on_menu_close`, `shutdown` (`module_stop`, no I/O). | P1 | Planned | host test with a mock |
| `CPL-REQ-004` | Items can be disabled with a reason (e.g. "No state to load") and are shown disabled, not hidden. | P1 | Planned | host test, UI review |
| `CPL-REQ-005` | System complication: Resume Game, Quit Game ([game shell](game-shell.md) `SHL-REQ-005`). | P0 | Planned | device test M3a |
| `CPL-REQ-006` | Settings complication: per-complication settings through the config store (`SYS-REQ-010`), edited in the hub. | P2 | Planned | device test |
| `CPL-REQ-007` | Each complication can be left out at build time with a flag; the hub adapts. | P2 | Planned | build with the flag off |
| `CPL-NFR-001` | A complication costs nothing while the hub is closed beyond its static data; it creates no thread unless it needs one. | P1 | Planned | code review, `make check` |

## C. Architecture and integration

```c
/* PROPOSED (ADR-009) */
typedef struct pw_complication {
    const char *id;     /* "states", "system", "settings" */
    int title;          /* string id (i18n) */
    int icon;           /* icon id (native asset or fallback) */
    int (*init)(void);
    int (*build_items)(pw_item_list *out); /* called on menu open */
    void (*on_menu_open)(void);
    void (*on_menu_close)(void);
    void (*shutdown)(void);
} pw_complication;
```

The hub calls `build_items` of every registered complication on open; an item carries a label,
icon, enabled flag + reason, and an action callback that runs in the menu thread with the game
frozen (held freeze available to it). Items that need the engine go through the engine owner lock
(`STE-REQ-011`).

## D. User experience and visual design

Complications appear as items (or item groups) in the hub ([UI spec](../ui/README.md)). Their
order and icons follow the XMB look; the user may later hide or reorder them (customization,
[ui/customization](../ui/customization.md)).

## E. Reference material

Custom Home's menu structure (behaviour only), the XMB's category/item model
([ui/references](../ui/references.md)).

## F. Implementation plan

Introduce the registry together with the XMB-style hub (M4): move Save/Load into
`complications/states/ui.c`, Resume/Quit into `complications/system/`; the hub iterates the
registry. Settings follows the config store.

## G. Testing and acceptance

Host tests of the item model and registry iteration; a "hello" test complication in a debug build
proves `CPL-REQ-001`.

## H. Risks and decisions

[ADR-009](../architecture/decisions.md#adr-009--complication-framework-and-ui-layering). Risk: over-engineering before a second real complication exists — keep the
interface minimal (the struct above) until Settings lands.
