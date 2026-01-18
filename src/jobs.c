#include "jobs.h"

void job_list_init(job_list_t *list) {
    list->items = NULL;
    list->count = 0;
    list->selected = 0;
}

void job_list_refresh(job_list_t *list) {
    if (list->items) {
        free_jobs(list->items);
    }
    list->count = get_jobs(&list->items);
    if (list->selected >= list->count) {
        list->selected = list->count > 0 ? list->count - 1 : 0;
    }
}

void job_list_free(job_list_t *list) {
    if (list->items) {
        free_jobs(list->items);
        list->items = NULL;
    }
    list->count = 0;
    list->selected = 0;
}

void job_list_move(job_list_t *list, int delta) {
    list->selected += delta;
    if (list->selected < 0) list->selected = 0;
    if (list->selected >= list->count) list->selected = list->count - 1;
}
