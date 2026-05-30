// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#define _POSIX_C_SOURCE 200809L
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

static _Noreturn void die_json(const char *error) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        fprintf(stderr, "db-probe: FATAL: cJSON alloc failed\n");
        printf("{\"ok\":false,\"error\":\"alloc failed\"}\n");
        exit(1);
    }
    cJSON_AddBoolToObject(root, "ok", 0);
    cJSON_AddStringToObject(root, "error", error);
    char *s = cJSON_PrintUnformatted(root);
    if (s) { printf("%s\n", s); free(s); }
    else   { printf("{\"ok\":false,\"error\":\"%s\"}\n", error); }
    cJSON_Delete(root);
    fprintf(stderr, "db-probe: %s\n", error);
    exit(1);
}

static _Noreturn void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [--help]\n"
        "\n"
        "Test database connectivity through db-proxy socket.\n"
        "Requires db-proxy daemon running on {scratch}/db-proxy.sock\n"
        "or $VEHIR_DB_PROXY_SOCK override.\n"
        "\n"
        "Exit: 0 = connected, 1 = failed, 2 = usage\n",
        prog);
    exit(2);
}

int main(int argc, char *argv[]) {
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 ||
                     strcmp(argv[1], "-h") == 0))
        usage(argv[0]);

    vehir_db *db = vehir_db_open();
    if (!db)
        die_json("vehir_db_open failed");

    vehir_db_result *r = vehir_db_query(db, "SELECT 1 AS test", NULL, 0);
    if (!r)
        die_json("vehir_db_query returned NULL");

    if (vehir_db_result_nrows(r) == 0) {
        cJSON *root = cJSON_CreateObject();
        if (root) {
            cJSON_AddBoolToObject(root, "ok", 0);
            cJSON_AddBoolToObject(root, "connected", 1);
            cJSON_AddStringToObject(root, "error", vehir_db_result_error(r));
            char *s = cJSON_PrintUnformatted(root);
            if (s) { printf("%s\n", s); free(s); }
            cJSON_Delete(root);
        }
        vehir_db_result_free(r);
        vehir_db_close(db);
        return 1;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) die_json("cJSON alloc failed");
    cJSON_AddBoolToObject(root, "ok", 1);
    cJSON_AddBoolToObject(root, "connected", 1);
    cJSON_AddNumberToObject(root, "nrows", vehir_db_result_nrows(r));
    cJSON_AddNumberToObject(root, "ncols", vehir_db_result_ncols(r));

    cJSON *cols = cJSON_AddArrayToObject(root, "columns");
    int ncols = vehir_db_result_ncols(r);
    for (int c = 0; c < ncols; c++)
        cJSON_AddItemToArray(cols,
            cJSON_CreateString(vehir_db_result_colname(r, c)));

    cJSON *rows = cJSON_AddArrayToObject(root, "rows");
    int nrows = vehir_db_result_nrows(r);
    for (int i = 0; i < nrows; i++) {
        cJSON *row_arr = cJSON_CreateArray();
        cJSON_AddItemToArray(rows, row_arr);
        for (int j = 0; j < ncols; j++) {
            if (vehir_db_result_isnull(r, i, j))
                cJSON_AddItemToArray(row_arr, cJSON_CreateNull());
            else
                cJSON_AddItemToArray(row_arr,
                    cJSON_CreateString(vehir_db_result_value(r, i, j)));
        }
    }

    char *out = cJSON_Print(root);
    if (!out) die_json("cJSON print failed");
    printf("%s\n", out);
    free(out);
    cJSON_Delete(root);
    vehir_db_result_free(r);
    vehir_db_close(db);
    return 0;
}
