#ifndef JOBS_H
#define JOBS_H

#include "cups_api.h"

typedef struct {
    job_info_t *items;
    int count;
    int selected;
} job_list_t;

void job_list_init(job_list_t *list);
void job_list_refresh(job_list_t *list);
void job_list_free(job_list_t *list);
void job_list_move(job_list_t *list, int delta);

#endif
