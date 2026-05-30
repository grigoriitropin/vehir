#define _POSIX_C_SOURCE 200809L
#include "pain.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vehir_pain_signal(vehir_db *db, const char *domain, const char *signal,
                       double severity, const char *source,
                       const char *description) {
    (void)source;
    if (!db || !domain || !signal) return -1;
    if (severity < 0.0) severity = 0.0;
    if (severity > 1.0) severity = 1.0;

    char sev_str[32];
    snprintf(sev_str, sizeof(sev_str), "%.4f", severity);

    const char *sql =
        "INSERT INTO pain_registry (domain, signal, severity, description) "
        "VALUES ($1, $2, $3::double precision, $4) "
        "ON CONFLICT (domain, signal) DO UPDATE "
        "SET severity = $3::double precision, "
        "    description = $4, "
        "    is_active = true, "
        "    updated_at = now()";

    const char *params[4] = { domain, signal, sev_str, description };
    return vehir_db_exec(db, sql, params, 4) >= 0 ? 0 : -1;
}

int vehir_pain_ok(vehir_db *db, const char *domain, const char *signal) {
    if (!db || !domain || !signal) return -1;

    const char *sql =
        "UPDATE pain_registry SET is_active = false, resolved_at = now() "
        "WHERE domain = $1 AND signal = $2 AND is_active = true";

    const char *params[2] = { domain, signal };
    return vehir_db_exec(db, sql, params, 2) >= 0 ? 0 : -1;
}

int vehir_pain_compute(vehir_db *db, vehir_pain_state *out) {
    if (!db || !out) return -1;
    memset(out, 0, sizeof(*out));

    vehir_db_result *r = vehir_db_query(db,
        "SELECT domain, signal, severity, description "
        "FROM compute_total_pain() "
        "ORDER BY severity DESC",
        NULL, 0);
    if (!r) return -1;

    int nrows = vehir_db_result_nrows(r);
    if (nrows == 0) {
        vehir_db_result_free(r);
        return 0;
    }
    if (nrows > VEHR_PAIN_MAX) nrows = VEHR_PAIN_MAX;

    double max_sev = 0.0;
    for (int i = 0; i < nrows; i++) {
        double sev = strtod(vehir_db_result_value(r, i, 2), NULL);
        if (sev > max_sev) max_sev = sev;

        vehir_pain_entry *e = &out->entries[i];
        snprintf(e->domain, sizeof(e->domain), "%s",
                 vehir_db_result_value(r, i, 0));
        snprintf(e->signal, sizeof(e->signal), "%s",
                 vehir_db_result_value(r, i, 1));
        e->severity = sev;
        snprintf(e->description, sizeof(e->description), "%s",
                 vehir_db_result_value(r, i, 3));
    }
    out->count = nrows;
    out->max_pain = max_sev;

    vehir_db_result_free(r);
    return 0;
}

const char *vehir_pain_mode(double pain) {
    if (pain <= 0.3)  return "ARCHITECT";
    if (pain <= 0.6)  return "MECHANIC";
    if (pain <= 0.85) return "TRAUMA_SURGEON";
    return "CRISIS";
}
