// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir Agent; authored by Vehir (autonomous agent)

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <cjson/cJSON.h>

static double now_unix(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        fprintf(stderr, "file-age: clock_gettime failed: %s\n", strerror(errno));
        printf("{\"ok\":false,\"error\":\"clock_gettime failed\"}\n");
        exit(1);
    }
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    size_t cap = 8192;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) { fclose(f); return NULL; }

    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += n;
        if (len + 1 >= cap) {
            cap *= 2;
            char *tmp = realloc(buf, cap);
            if (!tmp) { free(buf); fclose(f); return NULL; }
            buf = tmp;
        }
    }
    int err = ferror(f);
    fclose(f);
    if (err) { free(buf); return NULL; }

    buf[len] = '\0';
    return buf;
}

static _Noreturn void die_json(const char *error, const char *file, const char *field) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        fprintf(stderr, "file-age: FATAL: cJSON alloc failed\n");
        printf("{\"ok\":false,\"error\":\"alloc failed\"}\n");
        exit(1);
    }
    cJSON_AddBoolToObject(root, "ok", 0);
    cJSON_AddNumberToObject(root, "age_seconds", -1);
    if (file) cJSON_AddStringToObject(root, "file", file);
    if (field) cJSON_AddStringToObject(root, "field", field);
    cJSON_AddStringToObject(root, "error", error);

    char *s = cJSON_PrintUnformatted(root);
    if (s) {
        printf("%s\n", s);
        free(s);
    } else {
        printf("{\"ok\":false,\"error\":\"%s\"}\n", error);
    }
    cJSON_Delete(root);
    fprintf(stderr, "file-age: %s", error);
    if (file) fprintf(stderr, " (%s)", file);
    fprintf(stderr, "\n");
    exit(1);
}

static void print_result(int ok, double age, double max_age,
                          const char *file, const char *field) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        fprintf(stderr, "file-age: FATAL: cJSON alloc failed\n");
        exit(1);
    }
    cJSON_AddBoolToObject(root, "ok", ok);
    cJSON_AddNumberToObject(root, "age_seconds", age);
    cJSON_AddNumberToObject(root, "max_age_seconds", max_age);
    cJSON_AddStringToObject(root, "file", file);
    cJSON_AddStringToObject(root, "field", field);

    char *s = cJSON_PrintUnformatted(root);
    if (!s) {
        fprintf(stderr, "file-age: FATAL: cJSON print failed\n");
        cJSON_Delete(root);
        exit(1);
    }
    printf("%s\n", s);
    free(s);
    cJSON_Delete(root);
}

static int cmd_check(const char *path, double max_age, const char *field) {
    char *buf = read_file(path);
    if (!buf)
        die_json("file not found or unreadable", path, NULL);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root)
        die_json("invalid JSON", path, NULL);

    cJSON *ts_item = cJSON_GetObjectItemCaseSensitive(root, field);
    if (!ts_item || !cJSON_IsNumber(ts_item)) {
        cJSON_Delete(root);
        die_json("timestamp field not found or not a number", path, field);
    }

    double ts = ts_item->valuedouble;
    cJSON_Delete(root);

    if (ts <= 0.0)
        die_json("timestamp value <= 0", path, field);

    double t = now_unix();
    double age = t - ts;
    if (age < 0.0) age = 0.0;

    int ok = (age <= max_age);
    print_result(ok, age, max_age, path, field);
    return ok ? 0 : 1;
}

static _Noreturn void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s check <file> <max_age_seconds> [field_name]\n"
        "\n"
        "Check if a JSON file's timestamp field is within max_age_seconds.\n"
        "\n"
        "  file             Path to a JSON file containing a numeric timestamp\n"
        "  max_age_seconds  Maximum allowed age in seconds\n"
        "  field_name       JSON field name to read (default: \"timestamp\")\n"
        "\n"
        "Output: JSON {ok, age_seconds, max_age_seconds, file, field}\n"
        "Exit:   0 = within limits, 1 = stale/error, 2 = usage error\n",
        prog);
    exit(2);
}

int main(int argc, char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
        usage(argv[0]);

    if (strcmp(argv[1], "check") != 0) {
        fprintf(stderr, "file-age: unknown command '%s'\n", argv[1]);
        return 2;
    }

    if (argc < 4)
        usage(argv[0]);

    const char *path = argv[2];
    char *end = NULL;
    double max_age = strtod(argv[3], &end);
    if (end == argv[3] || max_age < 0.0) {
        fprintf(stderr, "file-age: invalid max_age_seconds '%s'\n", argv[3]);
        return 2;
    }

    const char *field = (argc > 4) ? argv[4] : "timestamp";
    return cmd_check(path, max_age, field);
}
