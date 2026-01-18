#ifndef UI_H
#define UI_H

#include <ncurses.h>
#include "printers.h"
#include "jobs.h"

typedef enum {
    PANEL_PRINTERS,
    PANEL_JOBS
} panel_t;

typedef enum {
    VIEW_MAIN,
    VIEW_DISCOVER
} view_t;

typedef enum {
    MODAL_NONE,
    MODAL_CONFIRM_DELETE,
    MODAL_CONFIRM_CANCEL_JOB
} modal_t;

typedef struct {
    WINDOW *header;
    WINDOW *main;
    WINDOW *footer;
    WINDOW *status;

    view_t current_view;
    panel_t active_panel;
    printer_list_t printers;
    job_list_t jobs;

    /* Discovery mode */
    char **discover_uris;
    char **discover_names;
    int discover_count;
    int discover_selected;

    /* Modal state */
    modal_t modal;
    char modal_msg[256];

    char status_msg[256];
    int running;
} ui_state_t;

void ui_init(ui_state_t *state);
void ui_cleanup(ui_state_t *state);
void ui_resize(ui_state_t *state);
void ui_draw(ui_state_t *state);
void ui_handle_input(ui_state_t *state, int ch);
void ui_set_status(ui_state_t *state, const char *fmt, ...);

#endif
