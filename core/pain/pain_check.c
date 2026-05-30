#define _POSIX_C_SOURCE 200809L
#include "pain.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

static _Noreturn void die_json(const char *error) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        fprintf(stderr, "pain-check: FATAL: alloc failed\n");
        printf("{\"ok\":false,\"error\":\"alloc failed\"}\n");
        exit(1);
    }
    cJSON_AddBoolToObject(root, "ok", 0);
    cJSON_AddStringToObject(root, "error", error);
    char *s = cJSON_PrintUnformatted(root);
    if (s) { printf("%s\n", s); free(s); }
    else   { printf("{\"ok\":false,\"error\":\"%s\"}\n", error); }
    cJSON_Delete(root);
    fprintf(stderr, "pain-check: %s\n", error);
    exit(1);
}

static _Noreturn void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [threshold]\n"
        "\n"
        "Check system pain level via compute_total_pain() through db-proxy.\n"
        "threshold defaults to 0.7. Excludes 'security' domain pains.\n"
        "\n"
        "Exit: 0 = all pain below threshold, 1 = pain exceeds, 2 = error\n",
        prog);
    exit(2);
}

int main(int argc, char *argv[]) {
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 ||
                     strcmp(argv[1], "-h") == 0))
        usage(argv[0]);

    double threshold = 0.7;
    if (argc > 1) {
        char *end;
        double v = strtod(argv[1], &end);
        if (end != argv[1] && v > 0.0 && v <= 1.0) threshold = v;
    }

    vehir_db *db = vehir_db_open();
    if (!db)
        die_json("cannot connect: db-proxy unreachable");

    vehir_pain_state state;
    if (vehir_pain_compute(db, &state) < 0) {
        vehir_db_close(db);
        die_json("compute_total_pain query failed");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", 1);
    cJSON_AddNumberToObject(root, "max_pain", state.max_pain);
    cJSON_AddStringToObject(root, "mode", vehir_pain_mode(state.max_pain));
    cJSON_AddNumberToObject(root, "count", state.count);

    cJSON *pains = cJSON_AddArrayToObject(root, "pains");
    int failed = 0;
    for (int i = 0; i < state.count; i++) {
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "domain", state.entries[i].domain);
        cJSON_AddStringToObject(p, "signal", state.entries[i].signal);
        cJSON_AddNumberToObject(p, "severity", state.entries[i].severity);
        cJSON_AddStringToObject(p, "description", state.entries[i].description);
        cJSON_AddItemToArray(pains, p);

        if (state.entries[i].severity >= threshold &&
            strcmp(state.entries[i].domain, "security") != 0)
            failed = 1;
    }

    char *out = cJSON_Print(root);
    if (!out) die_json("cJSON print failed");
    printf("%s\n", out);
    free(out);
    cJSON_Delete(root);

    vehir_db_close(db);
    return failed ? 1 : 0;
}
