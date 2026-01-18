# CLAUDE.md

## Build

```bash
make          # build
make clean    # clean build artifacts
```

Dependencies: libcups, ncursesw (or ncurses). Uses pkg-config.

## Architecture

```
main.c          Entry point, main loop (init → draw → input → cleanup)
ui.c/h          ncurses UI, all rendering and input handling
cups_api.c/h    CUPS backend - all printer/job operations
printers.c/h    Printer list state wrapper
jobs.c/h        Job list state wrapper
```

**Data flow**: `ui_state_t` holds all application state. The main loop calls `ui_draw()` then `ui_handle_input()`. Input handlers call into cups_api functions, then refresh the relevant list.

**Views**: Three views controlled by `view_t` enum - `VIEW_PRINTERS`, `VIEW_JOBS`, `VIEW_DISCOVER`. Each has its own draw function (`draw_printers`, `draw_jobs`, `draw_discover`) and input handler.

**Modals**: Confirmation dialogs use `modal_t` state. When modal is active, input is routed to `handle_modal_input()` instead of view handlers.

## Key patterns

- **List state**: `printer_list_t` and `job_list_t` wrap arrays with count and selected index. Use `*_list_refresh()` to reload from CUPS, `*_list_move()` for navigation.

- **Memory**: cups_api allocates arrays that callers must free with corresponding `free_*` functions. Lists handle their own memory in `*_list_free()`.

- **CUPS operations**: Some operations (set default, delete printer, add printer) shell out to `lpoptions`/`lpadmin` because the CUPS C API requires admin privileges. These use `system()` with single-quoted names.

- **Terminal resize**: Handled via `KEY_RESIZE` from ncurses. Windows are deleted and recreated in `ui_resize()`.

## UI layout

```
┌─────────────────────────────────────────┐
│ header (1 line) - title + tab bar       │
├─────────────────────────────────────────┤
│                                         │
│ main (flexible) - current view content  │
│                                         │
├─────────────────────────────────────────┤
│ footer (2 lines) - help + status        │
└─────────────────────────────────────────┘
```

## Adding new functionality

- **New view**: Add to `view_t` enum, create `draw_*` and `handle_*_input` functions, add cases in `ui_draw()` and `ui_handle_input()`.

- **New CUPS operation**: Add to cups_api.c/h. Prefer CUPS C API when possible; fall back to shell commands for privileged operations.

- **New keybinding**: Add to relevant `handle_*_input()` function, update help string in `draw_footer()`.
