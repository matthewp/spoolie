#include "ui.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define HEADER_HEIGHT 1
#define FOOTER_HEIGHT 2

static void draw_header(ui_state_t *state) {
    werase(state->header);
    wattron(state->header, A_REVERSE);

    int width = getmaxx(state->header);
    for (int i = 0; i < width; i++) {
        mvwaddch(state->header, 0, i, ' ');
    }

    mvwprintw(state->header, 0, 1, "spoolie");

    const char *tabs = "[P]rinters  [J]obs  [A]dd  [Q]uit";
    mvwprintw(state->header, 0, width - strlen(tabs) - 1, "%s", tabs);

    wattroff(state->header, A_REVERSE);
    wrefresh(state->header);
}

static void draw_footer(ui_state_t *state) {
    werase(state->footer);

    int width = getmaxx(state->footer);
    mvwhline(state->footer, 0, 0, ACS_HLINE, width);

    const char *help;
    switch (state->current_view) {
        case VIEW_PRINTERS:
            help = "j/k:navigate  Enter:set default  d:delete  r:refresh  q:quit";
            break;
        case VIEW_JOBS:
            help = "j/k:navigate  c:cancel job  r:refresh  q:quit";
            break;
        case VIEW_DISCOVER:
            help = "j/k:navigate  Enter:add printer  Esc:cancel";
            break;
    }
    wattron(state->footer, COLOR_PAIR(1));
    mvwprintw(state->footer, 1, 1, "%s", help);
    wattroff(state->footer, COLOR_PAIR(1));

    /* Status message on the right */
    if (state->status_msg[0]) {
        mvwprintw(state->footer, 1, width - strlen(state->status_msg) - 1,
                  "%s", state->status_msg);
    }

    wrefresh(state->footer);
}

static void draw_printers(ui_state_t *state) {
    werase(state->main);

    int width = getmaxx(state->main);

    wattron(state->main, A_BOLD);
    mvwprintw(state->main, 0, 1, "PRINTERS");
    wattroff(state->main, A_BOLD);
    mvwhline(state->main, 1, 0, ACS_HLINE, width);

    if (state->printers.count == 0) {
        mvwprintw(state->main, 3, 2, "No printers configured");
    } else {
        for (int i = 0; i < state->printers.count; i++) {
            printer_info_t *p = &state->printers.items[i];

            int y = i + 2;

            if (i == state->printers.selected) {
                wattron(state->main, A_REVERSE);
            }

            /* Clear the line */
            mvwhline(state->main, y, 0, ' ', width);

            /* Printer name */
            mvwprintw(state->main, y, 1, "%c %s",
                      i == state->printers.selected ? '>' : ' ',
                      p->name);

            /* Default marker */
            if (p->is_default) {
                wprintw(state->main, " (default)");
            }

            /* State */
            mvwprintw(state->main, y, 40, "%s", p->state);

            /* Model (truncated) */
            if (p->make_model[0]) {
                char model[30];
                strncpy(model, p->make_model, sizeof(model) - 1);
                model[sizeof(model) - 1] = '\0';
                mvwprintw(state->main, y, 55, "%s", model);
            }

            if (i == state->printers.selected) {
                wattroff(state->main, A_REVERSE);
            }
        }
    }

    wrefresh(state->main);
}

static void draw_jobs(ui_state_t *state) {
    werase(state->main);

    int width = getmaxx(state->main);

    wattron(state->main, A_BOLD);
    mvwprintw(state->main, 0, 1, "PRINT QUEUE");
    wattroff(state->main, A_BOLD);
    mvwhline(state->main, 1, 0, ACS_HLINE, width);

    if (state->jobs.count == 0) {
        mvwprintw(state->main, 3, 2, "No active print jobs");
    } else {
        /* Header */
        mvwprintw(state->main, 2, 1, "  %-6s %-20s %-30s %-10s",
                  "ID", "Printer", "Title", "State");

        for (int i = 0; i < state->jobs.count; i++) {
            job_info_t *j = &state->jobs.items[i];

            int y = i + 3;

            if (i == state->jobs.selected) {
                wattron(state->main, A_REVERSE);
            }

            mvwhline(state->main, y, 0, ' ', width);
            mvwprintw(state->main, y, 1, "%c %-6d %-20.20s %-30.30s %-10s",
                      i == state->jobs.selected ? '>' : ' ',
                      j->id, j->printer, j->title, j->state);

            if (i == state->jobs.selected) {
                wattroff(state->main, A_REVERSE);
            }
        }
    }

    wrefresh(state->main);
}

static void draw_discover(ui_state_t *state) {
    werase(state->main);

    int width = getmaxx(state->main);

    wattron(state->main, A_BOLD);
    mvwprintw(state->main, 0, 1, "DISCOVER PRINTERS");
    wattroff(state->main, A_BOLD);
    mvwhline(state->main, 1, 0, ACS_HLINE, width);

    if (state->discover_count == 0) {
        mvwprintw(state->main, 3, 2, "No network printers found");
    } else {
        for (int i = 0; i < state->discover_count; i++) {
            int y = i + 2;

            if (i == state->discover_selected) {
                wattron(state->main, A_REVERSE);
            }

            mvwhline(state->main, y, 0, ' ', width);
            mvwprintw(state->main, y, 1, "%c %s",
                      i == state->discover_selected ? '>' : ' ',
                      state->discover_uris[i]);

            if (i == state->discover_selected) {
                wattroff(state->main, A_REVERSE);
            }
        }
    }

    wrefresh(state->main);
}

void ui_init(ui_state_t *state) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    /* Initialize colors */
    start_color();
    use_default_colors();
    init_pair(1, COLOR_BLUE, -1);  /* Blue on default background */

    refresh();  /* Must refresh stdscr before subwindows will display */

    /* Create windows */
    int height, width;
    getmaxyx(stdscr, height, width);

    state->header = newwin(HEADER_HEIGHT, width, 0, 0);
    state->main = newwin(height - HEADER_HEIGHT - FOOTER_HEIGHT, width,
                         HEADER_HEIGHT, 0);
    state->footer = newwin(FOOTER_HEIGHT, width, height - FOOTER_HEIGHT, 0);

    state->current_view = VIEW_PRINTERS;
    state->status_msg[0] = '\0';
    state->running = 1;

    state->discover_uris = NULL;
    state->discover_names = NULL;
    state->discover_count = 0;
    state->discover_selected = 0;

    state->modal = MODAL_NONE;
    state->modal_msg[0] = '\0';

    printer_list_init(&state->printers);
    job_list_init(&state->jobs);

    printer_list_refresh(&state->printers);
    job_list_refresh(&state->jobs);
}

void ui_cleanup(ui_state_t *state) {
    delwin(state->header);
    delwin(state->main);
    delwin(state->footer);

    printer_list_free(&state->printers);
    job_list_free(&state->jobs);

    if (state->discover_uris) {
        free_discovered(state->discover_uris, state->discover_names,
                        state->discover_count);
    }

    endwin();
}

void ui_resize(ui_state_t *state) {
    /* Delete old windows */
    delwin(state->header);
    delwin(state->main);
    delwin(state->footer);

    /* ncurses already updated LINES/COLS when KEY_RESIZE was returned */
    int height, width;
    getmaxyx(stdscr, height, width);

    /* Recreate windows with new dimensions */
    state->header = newwin(HEADER_HEIGHT, width, 0, 0);
    state->main = newwin(height - HEADER_HEIGHT - FOOTER_HEIGHT, width,
                         HEADER_HEIGHT, 0);
    state->footer = newwin(FOOTER_HEIGHT, width, height - FOOTER_HEIGHT, 0);

    /* Force full redraw */
    clearok(curscr, TRUE);
}

static void draw_modal(ui_state_t *state) {
    int screen_h, screen_w;
    getmaxyx(stdscr, screen_h, screen_w);

    int modal_w = 50;
    int modal_h = 7;
    int start_y = (screen_h - modal_h) / 2;
    int start_x = (screen_w - modal_w) / 2;

    WINDOW *modal = newwin(modal_h, modal_w, start_y, start_x);
    box(modal, 0, 0);

    /* Title */
    const char *title = " Confirm ";
    mvwprintw(modal, 0, (modal_w - strlen(title)) / 2, "%s", title);

    /* Message */
    mvwprintw(modal, 2, 2, "%s", state->modal_msg);

    /* Instructions */
    wattron(modal, A_BOLD);
    mvwprintw(modal, 4, 2, "[y] Yes    [n] No");
    wattroff(modal, A_BOLD);

    wrefresh(modal);
    delwin(modal);
}

void ui_draw(ui_state_t *state) {
    draw_header(state);

    switch (state->current_view) {
        case VIEW_PRINTERS:
            draw_printers(state);
            break;
        case VIEW_JOBS:
            draw_jobs(state);
            break;
        case VIEW_DISCOVER:
            draw_discover(state);
            break;
    }

    draw_footer(state);

    /* Draw modal on top if active */
    if (state->modal != MODAL_NONE) {
        draw_modal(state);
    }
}

static void handle_printers_input(ui_state_t *state, int ch) {
    switch (ch) {
        case 'j':
        case KEY_DOWN:
            printer_list_move(&state->printers, 1);
            break;
        case 'k':
        case KEY_UP:
            printer_list_move(&state->printers, -1);
            break;
        case '\n':
        case KEY_ENTER:
            if (state->printers.count > 0) {
                printer_info_t *p = &state->printers.items[state->printers.selected];
                if (set_default_printer(p->name) == 0) {
                    ui_set_status(state, "Set %s as default", p->name);
                    printer_list_refresh(&state->printers);
                } else {
                    ui_set_status(state, "Failed to set default");
                }
            }
            break;
        case 'd':
            if (state->printers.count > 0) {
                printer_info_t *p = &state->printers.items[state->printers.selected];
                snprintf(state->modal_msg, sizeof(state->modal_msg),
                         "Delete printer '%s'?", p->name);
                state->modal = MODAL_CONFIRM_DELETE;
            }
            break;
        case 'r':
            printer_list_refresh(&state->printers);
            ui_set_status(state, "Refreshed");
            break;
    }
}

static void handle_jobs_input(ui_state_t *state, int ch) {
    switch (ch) {
        case 'j':
        case KEY_DOWN:
            job_list_move(&state->jobs, 1);
            break;
        case 'k':
        case KEY_UP:
            job_list_move(&state->jobs, -1);
            break;
        case 'c':
            if (state->jobs.count > 0) {
                job_info_t *j = &state->jobs.items[state->jobs.selected];
                snprintf(state->modal_msg, sizeof(state->modal_msg),
                         "Cancel job %d '%s'?", j->id, j->title);
                state->modal = MODAL_CONFIRM_CANCEL_JOB;
            }
            break;
        case 'r':
            job_list_refresh(&state->jobs);
            ui_set_status(state, "Refreshed");
            break;
    }
}

static void handle_discover_input(ui_state_t *state, int ch) {
    switch (ch) {
        case 'j':
        case KEY_DOWN:
            if (state->discover_selected < state->discover_count - 1)
                state->discover_selected++;
            break;
        case 'k':
        case KEY_UP:
            if (state->discover_selected > 0)
                state->discover_selected--;
            break;
        case '\n':
        case KEY_ENTER:
            if (state->discover_count > 0) {
                char *uri = state->discover_uris[state->discover_selected];
                char *name = state->discover_names[state->discover_selected];
                if (add_printer(name, uri) == 0) {
                    ui_set_status(state, "Added %s", name);
                    printer_list_refresh(&state->printers);
                } else {
                    ui_set_status(state, "Failed to add printer");
                }
                /* Return to printers view */
                state->current_view = VIEW_PRINTERS;
                free_discovered(state->discover_uris, state->discover_names,
                                state->discover_count);
                state->discover_uris = NULL;
                state->discover_names = NULL;
                state->discover_count = 0;
            }
            break;
        case 27: /* Escape */
            state->current_view = VIEW_PRINTERS;
            if (state->discover_uris) {
                free_discovered(state->discover_uris, state->discover_names,
                                state->discover_count);
                state->discover_uris = NULL;
                state->discover_names = NULL;
                state->discover_count = 0;
            }
            break;
    }
}

static void handle_modal_input(ui_state_t *state, int ch) {
    switch (ch) {
        case 'y':
        case 'Y':
            if (state->modal == MODAL_CONFIRM_DELETE) {
                printer_info_t *p = &state->printers.items[state->printers.selected];
                if (delete_printer(p->name) == 0) {
                    ui_set_status(state, "Deleted %s", p->name);
                    printer_list_refresh(&state->printers);
                } else {
                    ui_set_status(state, "Failed to delete printer");
                }
            } else if (state->modal == MODAL_CONFIRM_CANCEL_JOB) {
                job_info_t *j = &state->jobs.items[state->jobs.selected];
                if (cancel_job(j->id) == 0) {
                    ui_set_status(state, "Cancelled job %d", j->id);
                    job_list_refresh(&state->jobs);
                } else {
                    ui_set_status(state, "Failed to cancel job");
                }
            }
            state->modal = MODAL_NONE;
            break;
        case 'n':
        case 'N':
        case 27: /* Escape */
            state->modal = MODAL_NONE;
            ui_set_status(state, "Cancelled");
            break;
    }
}

void ui_handle_input(ui_state_t *state, int ch) {
    /* Handle modal input first */
    if (state->modal != MODAL_NONE) {
        handle_modal_input(state, ch);
        return;
    }

    /* Global keys */
    switch (ch) {
        case 'q':
        case 'Q':
            if (state->current_view == VIEW_DISCOVER) {
                state->current_view = VIEW_PRINTERS;
                if (state->discover_uris) {
                    free_discovered(state->discover_uris, state->discover_names,
                                    state->discover_count);
                    state->discover_uris = NULL;
                    state->discover_names = NULL;
                    state->discover_count = 0;
                }
            } else {
                state->running = 0;
            }
            return;
        case 'p':
        case 'P':
            if (state->current_view != VIEW_DISCOVER) {
                state->current_view = VIEW_PRINTERS;
            }
            return;
        case 'j' + 128: /* Alt-j - doesn't work well, use J */
        case 'J':
            if (state->current_view != VIEW_DISCOVER) {
                state->current_view = VIEW_JOBS;
                job_list_refresh(&state->jobs);
            }
            return;
        case 'a':
        case 'A':
            if (state->current_view != VIEW_DISCOVER) {
                state->current_view = VIEW_DISCOVER;
                state->discover_count = discover_printers(
                    &state->discover_uris, &state->discover_names);
                state->discover_selected = 0;
                if (state->discover_count == 0) {
                    ui_set_status(state, "No network printers found");
                }
            }
            return;
    }

    /* View-specific keys */
    switch (state->current_view) {
        case VIEW_PRINTERS:
            handle_printers_input(state, ch);
            break;
        case VIEW_JOBS:
            handle_jobs_input(state, ch);
            break;
        case VIEW_DISCOVER:
            handle_discover_input(state, ch);
            break;
    }
}

void ui_set_status(ui_state_t *state, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(state->status_msg, sizeof(state->status_msg), fmt, args);
    va_end(args);
}
