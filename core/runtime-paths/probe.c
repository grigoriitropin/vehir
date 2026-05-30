#define _POSIX_C_SOURCE 200809L
#include "runtime_paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

static _Noreturn void die_json(const char *error) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        fprintf(stderr, "runtime-paths-probe: FATAL: cJSON alloc failed\n");
        printf("{\"ok\":false,\"error\":\"alloc failed\"}\n");
        exit(1);
    }
    cJSON_AddBoolToObject(root, "ok", 0);
    cJSON_AddStringToObject(root, "error", error);
    char *s = cJSON_PrintUnformatted(root);
    if (s) { printf("%s\n", s); free(s); }
    else   { printf("{\"ok\":false,\"error\":\"%s\"}\n", error); }
    cJSON_Delete(root);
    fprintf(stderr, "runtime-paths-probe: %s\n", error);
    exit(1);
}

static void add_path(cJSON *obj, const char *key, const char *val) {
    cJSON_AddStringToObject(obj, key, val[0] ? val : "(unresolved)");
}

static _Noreturn void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [--help]\n"
        "\n"
        "Print all resolved Vehir runtime paths as JSON.\n"
        "Exit: 0 = success, 1 = resolution failed\n",
        prog);
    exit(2);
}

int main(int argc, char *argv[]) {
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 ||
                     strcmp(argv[1], "-h") == 0))
        usage(argv[0]);

    vehir_paths p;
    if (vehir_paths_init(&p) != 0)
        die_json("vehir_paths_init failed");

    cJSON *root = cJSON_CreateObject();
    if (!root)
        die_json("cJSON alloc failed");

    cJSON_AddBoolToObject(root, "ok", 1);
    cJSON_AddBoolToObject(root, "container", p.container);

    cJSON *dirs = cJSON_AddObjectToObject(root, "directories");
    if (!dirs)
        die_json("cJSON alloc failed");

    add_path(dirs, "root",    p.root);
    add_path(dirs, "bin",     p.bin);
    add_path(dirs, "scratch", p.scratch);
    add_path(dirs, "config",  p.config);
    add_path(dirs, "state",   p.state);

    static const char *test_services[] = {
        "vgw", "vsm-db", "vsm", "vsm-pain", "vsm-locks", NULL
    };

    cJSON *socks = cJSON_AddObjectToObject(root, "sockets");
    if (!socks)
        die_json("cJSON alloc failed");

    for (int i = 0; test_services[i]; i++) {
        char buf[VEHIR_PATH_MAX];
        if (vehir_paths_socket(&p, test_services[i], buf, sizeof(buf)) == 0)
            cJSON_AddStringToObject(socks, test_services[i], buf);
    }

    char *out = cJSON_Print(root);
    if (!out)
        die_json("cJSON print failed");

    printf("%s\n", out);
    free(out);
    cJSON_Delete(root);
    return 0;
}
