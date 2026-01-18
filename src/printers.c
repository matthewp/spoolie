#include "printers.h"

void printer_list_init(printer_list_t *list) {
    list->items = NULL;
    list->count = 0;
    list->selected = 0;
}

void printer_list_refresh(printer_list_t *list) {
    if (list->items) {
        free_printers(list->items);
    }
    list->count = get_printers(&list->items);
    if (list->selected >= list->count) {
        list->selected = list->count > 0 ? list->count - 1 : 0;
    }
}

void printer_list_free(printer_list_t *list) {
    if (list->items) {
        free_printers(list->items);
        list->items = NULL;
    }
    list->count = 0;
    list->selected = 0;
}

void printer_list_move(printer_list_t *list, int delta) {
    list->selected += delta;
    if (list->selected < 0) list->selected = 0;
    if (list->selected >= list->count) list->selected = list->count - 1;
}
