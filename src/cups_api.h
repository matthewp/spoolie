#ifndef CUPS_API_H
#define CUPS_API_H

#include <cups/cups.h>

/* Printer info structure */
typedef struct {
    char name[256];
    char make_model[256];
    char state[64];
    char location[256];
    int is_default;
    int accepting;
} printer_info_t;

/* Job info structure */
typedef struct {
    int id;
    char printer[256];
    char title[256];
    char user[64];
    char state[32];
    int size;
} job_info_t;

/* Get list of printers. Returns count, fills array. Caller must free with free_printers() */
int get_printers(printer_info_t **printers);
void free_printers(printer_info_t *printers);

/* Get print jobs. Returns count, fills array. Caller must free with free_jobs() */
int get_jobs(job_info_t **jobs);
void free_jobs(job_info_t *jobs);

/* Set default printer. Returns 0 on success */
int set_default_printer(const char *name);

/* Cancel a print job. Returns 0 on success */
int cancel_job(int job_id);

/* Delete a printer. Returns 0 on success */
int delete_printer(const char *name);

/* Discover network printers. Returns count, fills array of URIs */
int discover_printers(char ***uris, char ***names);
void free_discovered(char **uris, char **names, int count);

/* Add a printer. Returns 0 on success */
int add_printer(const char *name, const char *uri);

#endif
