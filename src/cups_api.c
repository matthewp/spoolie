#include "cups_api.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char *state_to_str(ipp_pstate_t state) {
    switch (state) {
        case IPP_PSTATE_IDLE:       return "idle";
        case IPP_PSTATE_PROCESSING: return "printing";
        case IPP_PSTATE_STOPPED:    return "stopped";
        default:                    return "unknown";
    }
}

static const char *job_state_to_str(ipp_jstate_t state) {
    switch (state) {
        case IPP_JSTATE_PENDING:    return "pending";
        case IPP_JSTATE_HELD:       return "held";
        case IPP_JSTATE_PROCESSING: return "printing";
        case IPP_JSTATE_STOPPED:    return "stopped";
        case IPP_JSTATE_CANCELED:   return "canceled";
        case IPP_JSTATE_ABORTED:    return "aborted";
        case IPP_JSTATE_COMPLETED:  return "completed";
        default:                    return "unknown";
    }
}

int get_printers(printer_info_t **printers) {
    cups_dest_t *dests;
    int num_dests = cupsGetDests(&dests);

    if (num_dests <= 0) {
        *printers = NULL;
        return 0;
    }

    *printers = calloc(num_dests, sizeof(printer_info_t));
    if (!*printers) {
        cupsFreeDests(num_dests, dests);
        return 0;
    }

    for (int i = 0; i < num_dests; i++) {
        cups_dest_t *dest = &dests[i];
        printer_info_t *p = &(*printers)[i];

        strncpy(p->name, dest->name, sizeof(p->name) - 1);
        p->is_default = dest->is_default;

        const char *val;

        val = cupsGetOption("printer-make-and-model", dest->num_options, dest->options);
        if (val) strncpy(p->make_model, val, sizeof(p->make_model) - 1);

        val = cupsGetOption("printer-location", dest->num_options, dest->options);
        if (val) strncpy(p->location, val, sizeof(p->location) - 1);

        val = cupsGetOption("printer-state", dest->num_options, dest->options);
        if (val) {
            ipp_pstate_t state = (ipp_pstate_t)atoi(val);
            strncpy(p->state, state_to_str(state), sizeof(p->state) - 1);
        }

        val = cupsGetOption("printer-is-accepting-jobs", dest->num_options, dest->options);
        p->accepting = val ? (strcmp(val, "true") == 0) : 1;
    }

    cupsFreeDests(num_dests, dests);
    return num_dests;
}

void free_printers(printer_info_t *printers) {
    free(printers);
}

int get_jobs(job_info_t **jobs) {
    cups_job_t *cups_jobs;
    int num_jobs = cupsGetJobs(&cups_jobs, NULL, 0, CUPS_WHICHJOBS_ACTIVE);

    if (num_jobs <= 0) {
        *jobs = NULL;
        return 0;
    }

    *jobs = calloc(num_jobs, sizeof(job_info_t));
    if (!*jobs) {
        cupsFreeJobs(num_jobs, cups_jobs);
        return 0;
    }

    for (int i = 0; i < num_jobs; i++) {
        cups_job_t *cj = &cups_jobs[i];
        job_info_t *j = &(*jobs)[i];

        j->id = cj->id;
        j->size = cj->size;
        strncpy(j->printer, cj->dest, sizeof(j->printer) - 1);
        strncpy(j->title, cj->title, sizeof(j->title) - 1);
        strncpy(j->user, cj->user, sizeof(j->user) - 1);
        strncpy(j->state, job_state_to_str(cj->state), sizeof(j->state) - 1);
    }

    cupsFreeJobs(num_jobs, cups_jobs);
    return num_jobs;
}

void free_jobs(job_info_t *jobs) {
    free(jobs);
}

int set_default_printer(const char *name) {
    /* cupsSetDefault requires admin - use lpoptions instead */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "lpoptions -d '%s' >/dev/null 2>&1", name);
    return system(cmd);
}

int cancel_job(int job_id) {
    return cupsCancelJob(NULL, job_id) ? 0 : -1;
}

int delete_printer(const char *name) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "lpadmin -x '%s' 2>/dev/null", name);
    return system(cmd);
}

int discover_printers(char ***uris, char ***names) {
    /* Use lpinfo to discover - more portable than raw CUPS discovery */
    FILE *fp = popen("lpinfo -v 2>/dev/null | grep -E '^network (socket|ipp|ipps)://'", "r");
    if (!fp) return 0;

    int count = 0;
    int capacity = 8;
    *uris = malloc(capacity * sizeof(char *));
    *names = malloc(capacity * sizeof(char *));

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        /* Line format: "network uri" */
        char *uri = strchr(line, ' ');
        if (!uri) continue;
        uri++;

        /* Trim newline */
        char *nl = strchr(uri, '\n');
        if (nl) *nl = '\0';

        if (count >= capacity) {
            capacity *= 2;
            *uris = realloc(*uris, capacity * sizeof(char *));
            *names = realloc(*names, capacity * sizeof(char *));
        }

        (*uris)[count] = strdup(uri);

        /* Generate a name from URI */
        char *name = strdup(uri);
        char *host = strstr(name, "://");
        if (host) {
            host += 3;
            char *slash = strchr(host, '/');
            if (slash) *slash = '\0';
            char *port = strchr(host, ':');
            if (port) *port = '\0';
        }
        (*names)[count] = name;

        count++;
    }

    pclose(fp);
    return count;
}

void free_discovered(char **uris, char **names, int count) {
    for (int i = 0; i < count; i++) {
        free(uris[i]);
        free(names[i]);
    }
    free(uris);
    free(names);
}

int add_printer(const char *name, const char *uri) {
    char cmd[1024];
    /* Try IPP Everywhere first, fall back to raw */
    if (strstr(uri, "ipp://") || strstr(uri, "ipps://")) {
        snprintf(cmd, sizeof(cmd),
            "lpadmin -p '%s' -E -v '%s' -m everywhere 2>/dev/null", name, uri);
    } else {
        snprintf(cmd, sizeof(cmd),
            "lpadmin -p '%s' -E -v '%s' 2>/dev/null", name, uri);
    }
    return system(cmd);
}
