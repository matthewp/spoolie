#ifndef PRINTERS_H
#define PRINTERS_H

#include "cups_api.h"

typedef struct {
    printer_info_t *items;
    int count;
    int selected;
} printer_list_t;

void printer_list_init(printer_list_t *list);
void printer_list_refresh(printer_list_t *list);
void printer_list_free(printer_list_t *list);
void printer_list_move(printer_list_t *list, int delta);

#endif
