// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Vehir

#include "broker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <cjson/cJSON.h>

static const char *config_dir = NULL;

void broker_set_dir(const char *dir) {
    config_dir = dir;
}

static char *default_path(void) {
    const char *home = getenv("HOME");
    if (!home || !home[0]) return NULL;
    const char *dir = config_dir ? config_dir : ".config/vehir/env";
    char *path = malloc(strlen(home) + strlen(dir) + 4);
    if (!path) return NULL;
    sprintf(path, "%s/%s", home, dir);
    return path;
}

br_cfg_t *broker_load(const char *path_override) {
    char *apath = NULL;
    const char *path = path_override;
    if (!path) {
        apath = default_path();
        path = apath;
    }
    if (!path) return NULL;

    struct stat st;
    if (stat(path, &st) != 0) {
        free(apath);
        return NULL;
    }
    if (st.st_mode & (S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) {
        free(apath);
        return NULL;  /* callers check NULL */
    }

    FILE *f = fopen(path, "r");
    if (!f) { free(apath); return NULL; }
    free(apath);

    br_cfg_t *cfg = calloc(1, sizeof(*cfg));
    if (!cfg) { fclose(f); return NULL; }
    int cap = 8;
    cfg->entries = malloc((size_t)cap * sizeof(br_kv_t));
    if (!cfg->entries) { free(cfg); fclose(f); return NULL; }

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = p, *val = eq + 1;
        size_t klen = strlen(key);
        while (klen > 0 && (key[klen-1] == ' ' || key[klen-1] == '\t')) key[--klen] = '\0';
        size_t vlen = strlen(val);
        while (vlen > 0 && (val[vlen-1] == '\n' || val[vlen-1] == '\r' || val[vlen-1] == ' ' || val[vlen-1] == '\t'))
            val[--vlen] = '\0';
        if (cfg->count >= cap) {
            cap *= 2;
            br_kv_t *tmp = realloc(cfg->entries, (size_t)cap * sizeof(br_kv_t));
            if (!tmp) { broker_free(cfg); fclose(f); return NULL; }
            cfg->entries = tmp;
        }
        cfg->entries[cfg->count].key = strdup(key);
        cfg->entries[cfg->count].val = strdup(val);
        if (!cfg->entries[cfg->count].key || !cfg->entries[cfg->count].val) {
            broker_free(cfg); fclose(f); return NULL;
        }
        cfg->count++;
    }
    fclose(f);
    return cfg;
}

const char *broker_get(const br_cfg_t *cfg, const char *key) {
    if (!cfg) return NULL;
    for (int i = 0; i < cfg->count; i++)
        if (strcmp(cfg->entries[i].key, key) == 0)
            return cfg->entries[i].val;
    return NULL;
}

void broker_free(br_cfg_t *cfg) {
    if (!cfg) return;
    for (int i = 0; i < cfg->count; i++) {
        free(cfg->entries[i].key);
        free(cfg->entries[i].val);
    }
    free(cfg->entries);
    free(cfg);
}

/* CLI tool */
#ifdef BROKER_CLI
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: broker get <key> | list | set <key> <val>\n");
        return 1;
    }

    if (strcmp(argv[1], "get") == 0 && argc >= 3) {
        br_cfg_t *cfg = broker_load(NULL);
        if (!cfg) {
            fprintf(stderr, "{\"ok\":false,\"error\":\"cannot load config\"}\n");
            return 1;
        }
        const char *v = broker_get(cfg, argv[2]);
        if (v) printf("%s\n", v);
        else { fprintf(stderr, "{\"ok\":false,\"error\":\"key not found\"}\n"); broker_free(cfg); return 1; }
        broker_free(cfg);
    } else if (strcmp(argv[1], "list") == 0) {
        br_cfg_t *cfg = broker_load(NULL);
        if (!cfg) {
            fprintf(stderr, "{\"ok\":false,\"error\":\"cannot load config\"}\n");
            return 1;
        }
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < cfg->count; i++) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, cfg->entries[i].key, cfg->entries[i].val);
            cJSON_AddItemToArray(arr, item);
        }
        char *s = cJSON_Print(arr);
        if (s) { printf("%s\n", s); free(s); }
        cJSON_Delete(arr);
        broker_free(cfg);
    } else {
        fprintf(stderr, "usage: broker get <key> | list | set <key> <val>\n");
        return 1;
    }
    return 0;
}
#endif /* BROKER_CLI */
