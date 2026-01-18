#include "ui.h"
#include "cups_api.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define HEADER_HEIGHT 1
#define FOOTER_HEIGHT 2

/* Args passed to discovery thread */
typedef struct {
    ui_state_t *state;
    int generation;
} discover_args_t;

/* Thread function for async printer discovery */
static void *discover_thread_func(void *arg) {
    discover_args_t *args = (discover_args_t *)arg;
    ui_state_t *state = args->state;
    int generation = args->generation;
    free(args);

    char **uris = NULL;
    char **names = NULL;
    int count = discover_printers(&uris, &names);

    /* Only update state if this discovery is still current */
    if (state->discover_generation == generation) {
        /* Free any previous results */
        if (state->discover_uris) {
            free_discovered(state->discover_uris, state->discover_names,
                            state->discover_count);
        }
        state->discover_uris = uris;
        state->discover_names = names;
        state->discover_count = count;
    } else {
        /* Stale discovery - free results */
        if (uris) {
            free_discovered(uris, names, count);
        }
    }

    return NULL;
}

static void draw_header(ui_state_t *state) {
    werase(state->header);
    wbkgd(state->header, COLOR_PAIR(4));

    int width = getmaxx(state->header);

    wattron(state->header, A_BOLD);
    mvwprintw(state->header, 0, 1, "spoolie");
    wattroff(state->header, A_BOLD);

    const char *tabs = "[Q]uit";
    mvwprintw(state->header, 0, width - strlen(tabs) - 1, "%s", tabs);

    wrefresh(state->header);
}

static void draw_footer(ui_state_t *state) {
    werase(state->footer);

    int width = getmaxx(state->footer);
    mvwhline(state->footer, 0, 0, ACS_HLINE, width);

    const char *help;
    switch (state->current_view) {
        case VIEW_MAIN:
            if (state->active_panel == PANEL_PRINTERS) {
                help = "Tab:switch  j/k:nav  Enter:default  d:delete  a:add  r:refresh  q:quit";
            } else {
                help = "Tab:switch  j/k:nav  c:cancel  r:refresh  q:quit";
            }
            break;
        case VIEW_DISCOVER:
            help = "j/k:navigate  Enter:add printer  q:cancel";
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

static void draw_panel_box(WINDOW *win, int y, int height, int width,
                           const char *title, int active) {
    int color = active ? 2 : 3;  /* Green if active, white if not */

    wattron(win, COLOR_PAIR(color));

    /* Draw box borders */
    mvwhline(win, y, 0, ACS_HLINE, width);
    mvwhline(win, y + height - 1, 0, ACS_HLINE, width);
    mvwvline(win, y, 0, ACS_VLINE, height);
    mvwvline(win, y, width - 1, ACS_VLINE, height);

    /* Corners */
    mvwaddch(win, y, 0, ACS_ULCORNER);
    mvwaddch(win, y, width - 1, ACS_URCORNER);
    mvwaddch(win, y + height - 1, 0, ACS_LLCORNER);
    mvwaddch(win, y + height - 1, width - 1, ACS_LRCORNER);

    /* Title */
    if (active) wattron(win, A_BOLD);
    mvwprintw(win, y, 2, " %s ", title);
    if (active) wattroff(win, A_BOLD);

    wattroff(win, COLOR_PAIR(color));
}

static void draw_main_panels(ui_state_t *state) {
    werase(state->main);

    int width = getmaxx(state->main);
    int height = getmaxy(state->main);

    int printers_height = height / 2;
    int jobs_height = height - printers_height;

    /* Draw printers panel */
    int printers_active = (state->active_panel == PANEL_PRINTERS);
    draw_panel_box(state->main, 0, printers_height, width, "Printers", printers_active);

    if (state->printers.count == 0) {
        mvwprintw(state->main, 2, 2, "No printers configured");
    } else {
        int max_items = printers_height - 2;
        int inner_width = width - 2;  /* Space inside the box */
        for (int i = 0; i < state->printers.count && i < max_items; i++) {
            printer_info_t *p = &state->printers.items[i];
            int y = i + 1;

            if (i == state->printers.selected && printers_active) {
                wattron(state->main, A_REVERSE);
                /* Fill the entire row */
                mvwhline(state->main, y, 1, ' ', inner_width);
            }

            mvwprintw(state->main, y, 2, "%c %-20.20s",
                      (i == state->printers.selected && printers_active) ? '>' : ' ',
                      p->name);

            if (p->is_default) {
                wprintw(state->main, " (default)");
            }

            int state_col = width / 2;
            mvwprintw(state->main, y, state_col, "%-10.10s", p->state);

            if (p->make_model[0]) {
                int model_col = state_col + 12;
                int model_max = width - model_col - 2;
                if (model_max > 0) {
                    mvwprintw(state->main, y, model_col, "%.*s", model_max, p->make_model);
                }
            }

            if (i == state->printers.selected && printers_active) {
                wattroff(state->main, A_REVERSE);
            }
        }
    }

    /* Draw jobs panel */
    int jobs_active = (state->active_panel == PANEL_JOBS);
    draw_panel_box(state->main, printers_height, jobs_height, width, "Jobs", jobs_active);

    if (state->jobs.count == 0) {
        mvwprintw(state->main, printers_height + 2, 2, "No active print jobs");
    } else {
        int max_items = jobs_height - 2;
        int inner_width = width - 2;
        for (int i = 0; i < state->jobs.count && i < max_items; i++) {
            job_info_t *j = &state->jobs.items[i];
            int y = printers_height + 1 + i;

            if (i == state->jobs.selected && jobs_active) {
                wattron(state->main, A_REVERSE);
                mvwhline(state->main, y, 1, ' ', inner_width);
            }

            /* Calculate column widths based on available space */
            int avail = inner_width - 4;  /* minus selector and padding */
            mvwprintw(state->main, y, 2, "%c %-6d %-15.15s %.*s",
                      (i == state->jobs.selected && jobs_active) ? '>' : ' ',
                      j->id, j->printer,
                      avail - 25 > 0 ? avail - 25 : 10, j->title);

            /* State on the right */
            mvwprintw(state->main, y, width - 12, "%-10.10s", j->state);

            if (i == state->jobs.selected && jobs_active) {
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

    if (state->discover_count < 0) {
        mvwprintw(state->main, 3, 2, "Discovering printers...");
    } else if (state->discover_count == 0) {
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
    init_pair(1, COLOR_BLUE, -1);    /* Blue on default background (footer) */
    init_pair(2, COLOR_GREEN, -1);   /* Green for active panel border */
    init_pair(3, COLOR_WHITE, -1);   /* Dim white for inactive panel border */
    init_pair(4, COLOR_WHITE, COLOR_BLUE);  /* Header: white on blue */

    refresh();  /* Must refresh stdscr before subwindows will display */

    /* Create windows */
    int height, width;
    getmaxyx(stdscr, height, width);

    state->header = newwin(HEADER_HEIGHT, width, 0, 0);
    state->main = newwin(height - HEADER_HEIGHT - FOOTER_HEIGHT, width,
                         HEADER_HEIGHT, 0);
    state->footer = newwin(FOOTER_HEIGHT, width, height - FOOTER_HEIGHT, 0);

    state->current_view = VIEW_MAIN;
    state->active_panel = PANEL_PRINTERS;
    state->status_msg[0] = '\0';
    state->running = 1;

    state->discover_uris = NULL;
    state->discover_names = NULL;
    state->discover_count = 0;
    state->discover_selected = 0;
    state->discover_generation = 0;

    state->modal = MODAL_NONE;
    state->modal_msg[0] = '\0';

    printer_list_init(&state->printers);
    job_list_init(&state->jobs);

    printer_list_refresh(&state->printers);
    job_list_refresh(&state->jobs);
}

void ui_cleanup(ui_state_t *state) {
    /* Invalidate any running discovery threads */
    state->discover_generation = -1;

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

void ui_poll(ui_state_t *state) {
    /* Check if discovery finished with no results */
    if (state->current_view == VIEW_DISCOVER &&
        state->discover_count == 0 && state->status_msg[0] == '\0') {
        ui_set_status(state, "No network printers found");
    }
}

void ui_resize(ui_state_t *state) {
    /* Delete old windows */
    delwin(state->header);
    delwin(state->main);
    delwin(state->footer);

    /* Clear screen and get new dimensions */
    clear();
    refresh();

    int height, width;
    getmaxyx(stdscr, height, width);

    /* Recreate windows with new dimensions */
    state->header = newwin(HEADER_HEIGHT, width, 0, 0);
    state->main = newwin(height - HEADER_HEIGHT - FOOTER_HEIGHT, width,
                         HEADER_HEIGHT, 0);
    state->footer = newwin(FOOTER_HEIGHT, width, height - FOOTER_HEIGHT, 0);

    /* Reapply header background color */
    wbkgd(state->header, COLOR_PAIR(4));
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
        case VIEW_MAIN:
            draw_main_panels(state);
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
                /* Return to main view and clean up */
                state->current_view = VIEW_MAIN;
                state->discover_generation++;
                free_discovered(state->discover_uris, state->discover_names,
                                state->discover_count);
                state->discover_uris = NULL;
                state->discover_names = NULL;
                state->discover_count = 0;
            }
            break;
        case 27: /* Escape */
            state->current_view = VIEW_MAIN;
            state->discover_generation++;
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
                state->current_view = VIEW_MAIN;
                /* Invalidate any running discovery so its results are discarded */
                state->discover_generation++;
            } else {
                state->running = 0;
            }
            return;
        case '\t':  /* Tab to switch panels */
            if (state->current_view == VIEW_MAIN) {
                state->active_panel = (state->active_panel == PANEL_PRINTERS)
                    ? PANEL_JOBS : PANEL_PRINTERS;
            }
            return;
        case 'a':
        case 'A':
            if (state->current_view != VIEW_DISCOVER) {
                state->current_view = VIEW_DISCOVER;
                state->discover_count = -1;  /* Show loading state */
                state->discover_selected = 0;
                state->discover_generation++;
                state->status_msg[0] = '\0';

                /* Start discovery in detached thread */
                discover_args_t *args = malloc(sizeof(discover_args_t));
                args->state = state;
                args->generation = state->discover_generation;

                pthread_t thread;
                pthread_create(&thread, NULL, discover_thread_func, args);
                pthread_detach(thread);
            }
            return;
        case 'r':
        case 'R':
            if (state->current_view == VIEW_MAIN) {
                printer_list_refresh(&state->printers);
                job_list_refresh(&state->jobs);
                ui_set_status(state, "Refreshed");
            }
            return;
    }

    /* View-specific keys */
    switch (state->current_view) {
        case VIEW_MAIN:
            if (state->active_panel == PANEL_PRINTERS) {
                handle_printers_input(state, ch);
            } else {
                handle_jobs_input(state, ch);
            }
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
